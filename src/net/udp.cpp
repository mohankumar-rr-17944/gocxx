#include <gocxx/net/udp.h>
#include <cstring>

// Platform-specific includes
#ifdef _WIN32
    #define NOMINMAX  // Prevent Windows from defining min/max macros
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")

    // Ensure Winsock is initialized (ref-counted, safe alongside TCP init)
    struct WinsockInitializerUDP {
        WinsockInitializerUDP() {
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
        }
        ~WinsockInitializerUDP() {
            WSACleanup();
        }
    };
    static WinsockInitializerUDP winsock_init_udp;

    #define SOCKET_ERROR_CODE WSAGetLastError()
    #define SOCKET_CLOSE closesocket
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>
    #define SOCKET_ERROR_CODE errno
    #define INVALID_SOCKET -1
    #define SOCKET_CLOSE ::close
#endif

namespace gocxx::net {

// Helper function to convert errno/WSAGetLastError to error (same as TCP)
static std::shared_ptr<gocxx::errors::Error> socketErrorToError(int err_code) {
    #ifdef _WIN32
    switch (err_code) {
        case WSAETIMEDOUT:
            return ErrTimeout;
        case WSAECONNRESET:
        case WSAENOTCONN:
            return ErrClosed;
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
            return ErrClosed;
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

// UDPAddr implementation
std::string UDPAddr::String() const {
    return ip + ":" + std::to_string(port);
}

// UDPConn implementation
UDPConn::UDPConn(int socket_fd, std::shared_ptr<UDPAddr> local_addr)
    : socket_fd_(socket_fd), local_addr_(local_addr), closed_(false) {
    read_deadline_ = std::chrono::system_clock::time_point::max();
    write_deadline_ = std::chrono::system_clock::time_point::max();
}

UDPConn::~UDPConn() {
    if (!closed_) {
        close();
    }
}

gocxx::base::Result<std::size_t> UDPConn::ReadFrom(
    uint8_t* buffer,
    std::size_t size,
    std::shared_ptr<Addr>& addr) {
    
    if (closed_) {
        return {0, ErrClosed};
    }
    
    sockaddr_in sender_addr;
    #ifdef _WIN32
    int sender_addr_len = sizeof(sender_addr);
    #else
    socklen_t sender_addr_len = sizeof(sender_addr);
    #endif
    
    #ifdef _WIN32
    int result = recvfrom(socket_fd_, reinterpret_cast<char*>(buffer), 
                         static_cast<int>(size), 0,
                         reinterpret_cast<sockaddr*>(&sender_addr), &sender_addr_len);
    #else
    ssize_t result = recvfrom(socket_fd_, buffer, size, 0,
                             reinterpret_cast<sockaddr*>(&sender_addr), &sender_addr_len);
    #endif
    
    if (result < 0) {
        return {0, socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    // Convert sender address
    char sender_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, INET_ADDRSTRLEN);
    int sender_port = ntohs(sender_addr.sin_port);
    
    addr = std::make_shared<UDPAddr>(sender_ip, sender_port);
    
    return {static_cast<std::size_t>(result), nullptr};
}

gocxx::base::Result<std::size_t> UDPConn::WriteTo(
    const uint8_t* buffer,
    std::size_t size,
    std::shared_ptr<Addr> addr) {
    
    if (closed_) {
        return {0, ErrClosed};
    }
    
    // Cast to UDPAddr
    auto udp_addr = std::dynamic_pointer_cast<UDPAddr>(addr);
    if (!udp_addr) {
        return {0, gocxx::errors::New("address must be UDPAddr")};
    }
    
    return WriteToUDP(buffer, size, udp_addr);
}

std::shared_ptr<Addr> UDPConn::LocalAddr() {
    return local_addr_;
}

gocxx::base::Result<void> UDPConn::SetReadDeadline(std::chrono::system_clock::time_point deadline) {
    read_deadline_ = deadline;
    // TODO: Implement actual deadline setting
    return {};
}

gocxx::base::Result<void> UDPConn::SetWriteDeadline(std::chrono::system_clock::time_point deadline) {
    write_deadline_ = deadline;
    // TODO: Implement actual deadline setting
    return {};
}

gocxx::base::Result<void> UDPConn::SetDeadline(std::chrono::system_clock::time_point deadline) {
    read_deadline_ = deadline;
    write_deadline_ = deadline;
    // TODO: Implement actual deadline setting
    return {};
}

void UDPConn::close() {
    if (!closed_) {
        SOCKET_CLOSE(socket_fd_);
        closed_ = true;
    }
}

gocxx::base::Result<std::size_t> UDPConn::Read(uint8_t* buffer, std::size_t size) {
    if (closed_) {
        return {0, ErrClosed};
    }
    
    #ifdef _WIN32
    int result = recv(socket_fd_, reinterpret_cast<char*>(buffer), static_cast<int>(size), 0);
    #else
    ssize_t result = recv(socket_fd_, buffer, size, 0);
    #endif
    
    if (result < 0) {
        return {0, socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    return {static_cast<std::size_t>(result), nullptr};
}

gocxx::base::Result<std::size_t> UDPConn::Write(const uint8_t* buffer, std::size_t size) {
    if (closed_) {
        return {0, ErrClosed};
    }
    
    #ifdef _WIN32
    int result = send(socket_fd_, reinterpret_cast<const char*>(buffer), static_cast<int>(size), 0);
    #else
    ssize_t result = send(socket_fd_, buffer, size, 0);
    #endif
    
    if (result < 0) {
        return {0, socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    return {static_cast<std::size_t>(result), nullptr};
}

gocxx::base::Result<std::size_t> UDPConn::ReadFromUDP(
    uint8_t* buffer,
    std::size_t size,
    std::shared_ptr<UDPAddr>& addr) {
    
    std::shared_ptr<Addr> base_addr;
    auto result = ReadFrom(buffer, size, base_addr);
    
    if (result.Ok()) {
        addr = std::dynamic_pointer_cast<UDPAddr>(base_addr);
    }
    
    return result;
}

gocxx::base::Result<std::size_t> UDPConn::WriteToUDP(
    const uint8_t* buffer,
    std::size_t size,
    std::shared_ptr<UDPAddr> addr) {
    
    if (closed_) {
        return {0, ErrClosed};
    }
    
    sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(addr->port);
    inet_pton(AF_INET, addr->ip.c_str(), &dest_addr.sin_addr);
    
    #ifdef _WIN32
    int result = sendto(socket_fd_, reinterpret_cast<const char*>(buffer),
                       static_cast<int>(size), 0,
                       reinterpret_cast<sockaddr*>(&dest_addr), sizeof(dest_addr));
    #else
    ssize_t result = sendto(socket_fd_, buffer, size, 0,
                           reinterpret_cast<sockaddr*>(&dest_addr), sizeof(dest_addr));
    #endif
    
    if (result < 0) {
        return {0, socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    return {static_cast<std::size_t>(result), nullptr};
}

// ResolveUDPAddr implementation
gocxx::base::Result<std::shared_ptr<UDPAddr>> ResolveUDPAddr(
    const std::string& network,
    const std::string& address) {
    
    if (network != "udp" && network != "udp4" && network != "udp6") {
        return {nullptr, gocxx::errors::New("unsupported network type: " + network)};
    }
    
    std::string host;
    int port;
    
    if (!parseAddress(address, host, port)) {
        return {nullptr, ErrInvalidAddr};
    }
    
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    
    if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0) {
        return {nullptr, gocxx::errors::New("cannot resolve address: " + host)};
    }
    
    char ip_str[INET_ADDRSTRLEN];
    sockaddr_in* addr_in = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    inet_ntop(AF_INET, &addr_in->sin_addr, ip_str, INET_ADDRSTRLEN);
    
    auto udp_addr = std::make_shared<UDPAddr>(ip_str, port);
    
    freeaddrinfo(result);
    
    return {udp_addr, nullptr};
}

// DialUDP implementation
gocxx::base::Result<std::shared_ptr<UDPConn>> DialUDP(
    const std::string& network,
    std::shared_ptr<UDPAddr> local_addr,
    std::shared_ptr<UDPAddr> remote_addr) {
    
    if (network != "udp" && network != "udp4" && network != "udp6") {
        return {nullptr, gocxx::errors::New("unsupported network type: " + network)};
    }
    
    // Create socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        return {nullptr, socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    // Bind to local address if specified
    if (local_addr) {
        sockaddr_in local;
        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_port = htons(local_addr->port);
        inet_pton(AF_INET, local_addr->ip.c_str(), &local.sin_addr);
        
        if (bind(sock, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) {
            SOCKET_CLOSE(sock);
            return {nullptr, socketErrorToError(SOCKET_ERROR_CODE)};
        }
    }
    
    // Connect to remote address if specified (for connected UDP)
    if (remote_addr) {
        sockaddr_in remote;
        memset(&remote, 0, sizeof(remote));
        remote.sin_family = AF_INET;
        remote.sin_port = htons(remote_addr->port);
        inet_pton(AF_INET, remote_addr->ip.c_str(), &remote.sin_addr);
        
        if (connect(sock, reinterpret_cast<sockaddr*>(&remote), sizeof(remote)) != 0) {
            SOCKET_CLOSE(sock);
            return {nullptr, socketErrorToError(SOCKET_ERROR_CODE)};
        }
    }
    
    // Get actual local address
    sockaddr_in actual_local;
    #ifdef _WIN32
    int local_len = sizeof(actual_local);
    #else
    socklen_t local_len = sizeof(actual_local);
    #endif
    
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&actual_local), &local_len) != 0) {
        SOCKET_CLOSE(sock);
        return {nullptr, socketErrorToError(SOCKET_ERROR_CODE)};
    }
    
    char local_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &actual_local.sin_addr, local_ip, INET_ADDRSTRLEN);
    int local_port = ntohs(actual_local.sin_port);
    
    auto actual_local_addr = std::make_shared<UDPAddr>(local_ip, local_port);
    auto conn = std::make_shared<UDPConn>(sock, actual_local_addr);
    
    return {conn, nullptr};
}

// ListenUDP implementation
gocxx::base::Result<std::shared_ptr<UDPConn>> ListenUDP(
    const std::string& network,
    std::shared_ptr<UDPAddr> local_addr) {
    
    if (!local_addr) {
        return {nullptr, gocxx::errors::New("local address is required")};
    }
    
    return DialUDP(network, local_addr, nullptr);
}

// ListenUDPSimple implementation
gocxx::base::Result<std::shared_ptr<UDPConn>> ListenUDPSimple(const std::string& address) {
    auto addr_result = ResolveUDPAddr("udp", address);
    if (addr_result.Failed()) {
        return {nullptr, addr_result.err};
    }
    
    return ListenUDP("udp", addr_result.value);
}

} // namespace gocxx::net
