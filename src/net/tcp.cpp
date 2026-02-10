#include <gocxx/net/tcp.h>
#include <sstream>
#include <cstring>
#include <algorithm>

// Platform-specific includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    
    // Ensure Winsock is initialized
    struct WinsockInitializer {
        WinsockInitializer() {
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
        }
        ~WinsockInitializer() {
            WSACleanup();
        }
    };
    static WinsockInitializer winsock_init;
    
    #define SOCKET_ERROR_CODE WSAGetLastError()
    #define SOCKET_CLOSE closesocket
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    
    #define SOCKET_ERROR_CODE errno
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define SOCKET_CLOSE ::close
#endif

namespace gocxx::net {

// Helper function to convert errno/WSAGetLastError to error
static std::shared_ptr<gocxx::errors::Error> socketErrorToError(int err_code) {
    #ifdef _WIN32
    switch (err_code) {
        case WSAETIMEDOUT:
            return ErrTimeout;
        case WSAECONNRESET:
        case WSAENOTCONN:
            return ErrClosed;
        case WSAEADDRINUSE:
            return gocxx::errors::New("address already in use");
        case WSAEADDRNOTAVAIL:
            return gocxx::errors::New("cannot assign requested address");
        default: {
            char buf[256];
            FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err_code,
                          0, buf, sizeof(buf), nullptr);
            return gocxx::errors::New(std::string("socket error: ") + buf);
        }
    }
    #else
    switch (err_code) {
        case ETIMEDOUT:
            return ErrTimeout;
        case ECONNRESET:
        case ENOTCONN:
        case EPIPE:
            return ErrClosed;
        case EADDRINUSE:
            return gocxx::errors::New("address already in use");
        case EADDRNOTAVAIL:
            return gocxx::errors::New("cannot assign requested address");
        default:
            return gocxx::errors::New(std::string("socket error: ") + strerror(err_code));
    }
    #endif
}

// Helper to parse address into host and port
static bool parseAddress(const std::string& address, std::string& host, int& port) {
    size_t colon_pos = address.rfind(':');
    if (colon_pos == std::string::npos) {
        return false;
    }
    
    host = address.substr(0, colon_pos);
    std::string port_str = address.substr(colon_pos + 1);
    
    // Handle empty host (means listen on all interfaces)
    if (host.empty()) {
        host = "0.0.0.0";
    }
    
    try {
        port = std::stoi(port_str);
        if (port < 0 || port > 65535) {
            return false;
        }
    } catch (...) {
        return false;
    }
    
    return true;
}

// TCPAddr implementation
std::string TCPAddr::String() const {
    return ip + ":" + std::to_string(port);
}

// TCPConn implementation
TCPConn::TCPConn(int socket_fd, std::shared_ptr<TCPAddr> local, std::shared_ptr<TCPAddr> remote)
    : socket_fd_(socket_fd), local_addr_(local), remote_addr_(remote), closed_(false) {
    read_deadline_ = std::chrono::system_clock::time_point::max();
    write_deadline_ = std::chrono::system_clock::time_point::max();
}

TCPConn::~TCPConn() {
    if (!closed_) {
        close();
    }
}

gocxx::base::Result<std::size_t> TCPConn::Read(uint8_t* buffer, std::size_t size) {
    if (closed_) {
        return {0, ErrClosed};
    }
    
    // TODO: Handle read deadline with select/poll
    
    #ifdef _WIN32
    int result = recv(socket_fd_, reinterpret_cast<char*>(buffer), static_cast<int>(size), 0);
    #else
    ssize_t result = recv(socket_fd_, buffer, size, 0);
    #endif
    
    if (result < 0) {
        return {0, socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    if (result == 0) {
        // Connection closed by peer
        return {0, ErrClosed};
    }
    
    return {static_cast<std::size_t>(result), nullptr};
}

gocxx::base::Result<std::size_t> TCPConn::Write(const uint8_t* buffer, std::size_t size) {
    if (closed_) {
        return {0, ErrClosed};
    }
    
    // TODO: Handle write deadline with select/poll
    
    #ifdef _WIN32
    int result = send(socket_fd_, reinterpret_cast<const char*>(buffer), static_cast<int>(size), 0);
    #else
    ssize_t result = send(socket_fd_, buffer, size, MSG_NOSIGNAL);
    #endif
    
    if (result < 0) {
        return {0, socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    return {static_cast<std::size_t>(result), nullptr};
}

void TCPConn::close() {
    if (!closed_) {
        SOCKET_CLOSE(socket_fd_);
        closed_ = true;
    }
}

std::shared_ptr<Addr> TCPConn::LocalAddr() {
    return local_addr_;
}

std::shared_ptr<Addr> TCPConn::RemoteAddr() {
    return remote_addr_;
}

gocxx::base::Result<void> TCPConn::SetReadDeadline(std::chrono::system_clock::time_point deadline) {
    read_deadline_ = deadline;
    // TODO: Implement actual deadline setting with socket options
    return {};
}

gocxx::base::Result<void> TCPConn::SetWriteDeadline(std::chrono::system_clock::time_point deadline) {
    write_deadline_ = deadline;
    // TODO: Implement actual deadline setting with socket options
    return {};
}

gocxx::base::Result<void> TCPConn::SetDeadline(std::chrono::system_clock::time_point deadline) {
    read_deadline_ = deadline;
    write_deadline_ = deadline;
    // TODO: Implement actual deadline setting with socket options
    return {};
}

gocxx::base::Result<void> TCPConn::CloseRead() {
    if (closed_) {
        return {ErrClosed};
    }
    
    #ifdef _WIN32
    if (shutdown(socket_fd_, SD_RECEIVE) != 0) {
    #else
    if (shutdown(socket_fd_, SHUT_RD) != 0) {
    #endif
        return {socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    return {};
}

gocxx::base::Result<void> TCPConn::CloseWrite() {
    if (closed_) {
        return {ErrClosed};
    }
    
    #ifdef _WIN32
    if (shutdown(socket_fd_, SD_SEND) != 0) {
    #else
    if (shutdown(socket_fd_, SHUT_WR) != 0) {
    #endif
        return {socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    return {};
}

// TCPListener implementation
TCPListener::TCPListener(int socket_fd, std::shared_ptr<TCPAddr> local_addr)
    : socket_fd_(socket_fd), local_addr_(local_addr), closed_(false) {}

TCPListener::~TCPListener() {
    if (!closed_) {
        Close();
    }
}

gocxx::base::Result<std::shared_ptr<Conn>> TCPListener::Accept() {
    if (closed_) {
        return {nullptr, ErrClosed};
    }
    
    sockaddr_in client_addr;
    #ifdef _WIN32
    int client_addr_len = sizeof(client_addr);
    #else
    socklen_t client_addr_len = sizeof(client_addr);
    #endif
    
    int client_socket = accept(socket_fd_, 
                               reinterpret_cast<sockaddr*>(&client_addr),
                               &client_addr_len);
    
    if (client_socket == INVALID_SOCKET) {
        return {nullptr, socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    // Get client address info
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);
    
    auto remote_addr = std::make_shared<TCPAddr>(client_ip, client_port);
    auto conn = std::make_shared<TCPConn>(client_socket, local_addr_, remote_addr);
    
    return {conn, nullptr};
}

gocxx::base::Result<void> TCPListener::Close() {
    if (closed_) {
        return {ErrClosed};
    }
    
    SOCKET_CLOSE(socket_fd_);
    closed_ = true;
    return {};
}

std::shared_ptr<Addr> TCPListener::Address() {
    return local_addr_;
}

// ResolveTCPAddr implementation
gocxx::base::Result<std::shared_ptr<TCPAddr>> ResolveTCPAddr(
    const std::string& network,
    const std::string& address) {
    
    if (network != "tcp" && network != "tcp4" && network != "tcp6") {
        return {nullptr, gocxx::errors::New("unsupported network type: " + network)};
    }
    
    std::string host;
    int port;
    
    if (!parseAddress(address, host, port)) {
        return {nullptr, ErrInvalidAddr};
    }
    
    // Simple resolution - just use the IP as-is or resolve hostname
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;  // IPv4
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0) {
        return {nullptr, gocxx::errors::New("cannot resolve address: " + host)};
    }
    
    char ip_str[INET_ADDRSTRLEN];
    sockaddr_in* addr_in = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    inet_ntop(AF_INET, &addr_in->sin_addr, ip_str, INET_ADDRSTRLEN);
    
    auto tcp_addr = std::make_shared<TCPAddr>(ip_str, port);
    
    freeaddrinfo(result);
    
    return {tcp_addr, nullptr};
}

// DialTCP implementation
gocxx::base::Result<std::shared_ptr<TCPConn>> DialTCP(
    const std::string& network,
    const std::string& address) {
    
    auto addr_result = ResolveTCPAddr(network, address);
    if (addr_result.Failed()) {
        return {nullptr, addr_result.err};
    }
    
    auto remote_addr = addr_result.value;
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        return {nullptr, socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    // Connect to remote
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(remote_addr->port);
    inet_pton(AF_INET, remote_addr->ip.c_str(), &server_addr.sin_addr);
    
    if (connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) != 0) {
        SOCKET_CLOSE(sock);
        return {nullptr, socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    // Get local address
    sockaddr_in local_addr;
    #ifdef _WIN32
    int local_addr_len = sizeof(local_addr);
    #else
    socklen_t local_addr_len = sizeof(local_addr);
    #endif
    
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&local_addr), &local_addr_len) != 0) {
        SOCKET_CLOSE(sock);
        return {nullptr, socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    char local_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, INET_ADDRSTRLEN);
    int local_port = ntohs(local_addr.sin_port);
    
    auto local_tcp_addr = std::make_shared<TCPAddr>(local_ip, local_port);
    auto conn = std::make_shared<TCPConn>(sock, local_tcp_addr, remote_addr);
    
    return {conn, nullptr};
}

// Dial implementation
gocxx::base::Result<std::shared_ptr<Conn>> Dial(const std::string& address) {
    auto result = DialTCP("tcp", address);
    if (result.Failed()) {
        return {nullptr, result.err};
    }
    return {result.value, nullptr};
}

// ListenTCP implementation
gocxx::base::Result<std::shared_ptr<TCPListener>> ListenTCP(
    const std::string& network,
    const std::string& address) {
    
    if (network != "tcp" && network != "tcp4" && network != "tcp6") {
        return {nullptr, gocxx::errors::New("unsupported network type: " + network)};
    }
    
    std::string host;
    int port;
    
    if (!parseAddress(address, host, port)) {
        return {nullptr, ErrInvalidAddr};
    }
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        return {nullptr, socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    // Set SO_REUSEADDR
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    
    // Bind to address
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (host == "0.0.0.0") {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);
    }
    
    if (bind(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) != 0) {
        SOCKET_CLOSE(sock);
        return {nullptr, socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    // Listen
    if (listen(sock, 128) != 0) {  // 128 is a common backlog size
        SOCKET_CLOSE(sock);
        return {nullptr, socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    auto local_addr = std::make_shared<TCPAddr>(host, port);
    auto listener = std::make_shared<TCPListener>(sock, local_addr);
    
    return {listener, nullptr};
}

// Listen implementation
gocxx::base::Result<std::shared_ptr<Listener>> Listen(const std::string& address) {
    auto result = ListenTCP("tcp", address);
    if (result.Failed()) {
        return {nullptr, result.err};
    }
    return {result.value, nullptr};
}

} // namespace gocxx::net
