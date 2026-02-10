#pragma once

/**
 * @file tcp.h
 * @brief TCP networking support (Go-style)
 * 
 * Provides TCP client and server functionality:
 * - TCPAddr: TCP network address
 * - TCPConn: TCP connection
 * - TCPListener: TCP listener
 * - Dial, Listen: Connection establishment
 */

#include <gocxx/net/net.h>
#include <string>
#include <memory>

namespace gocxx::net {

/**
 * @brief TCP network address
 */
class TCPAddr : public Addr {
public:
    std::string ip;    ///< IP address
    int port;          ///< Port number
    
    TCPAddr() : port(0) {}
    TCPAddr(const std::string& ip, int port) : ip(ip), port(port) {}
    
    std::string Network() const override { return "tcp"; }
    std::string String() const override;
};

/**
 * @brief TCP connection
 * 
 * Implements a TCP network connection with Reader, Writer, and Closer interfaces.
 */
class TCPConn : public Conn {
public:
    TCPConn(int socket_fd, std::shared_ptr<TCPAddr> local, std::shared_ptr<TCPAddr> remote);
    
protected:
    // Protected constructor for derived classes (like TLSConn)
    TCPConn(int socket_fd);
    
public:
    virtual ~TCPConn();
    
    // io::Reader interface
    gocxx::base::Result<std::size_t> Read(uint8_t* buffer, std::size_t size) override;
    
    // io::Writer interface
    gocxx::base::Result<std::size_t> Write(const uint8_t* buffer, std::size_t size) override;
    
    // io::Closer interface
    void close() override;
    
    // Conn interface
    std::shared_ptr<Addr> LocalAddr() override;
    std::shared_ptr<Addr> RemoteAddr() override;
    
    gocxx::base::Result<void> SetReadDeadline(
        std::chrono::system_clock::time_point deadline) override;
    gocxx::base::Result<void> SetWriteDeadline(
        std::chrono::system_clock::time_point deadline) override;
    gocxx::base::Result<void> SetDeadline(
        std::chrono::system_clock::time_point deadline) override;
    
    /**
     * @brief Shuts down the reading side of the connection
     */
    gocxx::base::Result<void> CloseRead();
    
    /**
     * @brief Shuts down the writing side of the connection
     */
    gocxx::base::Result<void> CloseWrite();
    
    /**
     * @brief Gets the underlying socket file descriptor
     * @note This is needed for TLS wrapping
     */
    int GetFD() const { return socket_fd_; }

protected:
    int socket_fd_;
    std::shared_ptr<TCPAddr> local_addr_;
    std::shared_ptr<TCPAddr> remote_addr_;
    bool closed_;
    
    std::chrono::system_clock::time_point read_deadline_;
    std::chrono::system_clock::time_point write_deadline_;
};

/**
 * @brief TCP listener
 * 
 * Accepts incoming TCP connections.
 */
class TCPListener : public Listener {
public:
    TCPListener(int socket_fd, std::shared_ptr<TCPAddr> local_addr);
    
protected:
    // Protected constructor for derived classes (like TLSListener)
    TCPListener(int socket_fd);
    
public:
    virtual ~TCPListener();
    
    gocxx::base::Result<std::shared_ptr<Conn>> Accept() override;
    gocxx::base::Result<void> Close() override;
    std::shared_ptr<Addr> Address() override;
    
    /**
     * @brief Gets the underlying socket file descriptor
     * @note This is needed for TLS wrapping
     */
    int GetFD() const { return socket_fd_; }

protected:
    int socket_fd_;
    std::shared_ptr<TCPAddr> local_addr_;
    bool closed_;
};

/**
 * @brief Resolves a TCP address from a string
 * 
 * @param network Network type ("tcp", "tcp4", "tcp6")
 * @param address Address string (e.g., "localhost:8080", "192.168.1.1:80")
 * @return Result containing TCPAddr or an error
 */
gocxx::base::Result<std::shared_ptr<TCPAddr>> ResolveTCPAddr(
    const std::string& network, 
    const std::string& address);

/**
 * @brief Dials a TCP connection to the specified address
 * 
 * Similar to Go's net.Dial("tcp", address)
 * 
 * @param network Network type ("tcp", "tcp4", "tcp6")
 * @param address Address string (e.g., "localhost:8080")
 * @return Result containing TCPConn or an error
 */
gocxx::base::Result<std::shared_ptr<TCPConn>> DialTCP(
    const std::string& network,
    const std::string& address);

/**
 * @brief Convenience function to dial a TCP connection
 * 
 * @param address Address string (e.g., "localhost:8080")
 * @return Result containing connection or an error
 */
gocxx::base::Result<std::shared_ptr<Conn>> Dial(const std::string& address);

/**
 * @brief Listens for incoming TCP connections on the specified address
 * 
 * Similar to Go's net.Listen("tcp", address)
 * 
 * @param network Network type ("tcp", "tcp4", "tcp6")
 * @param address Address string (e.g., ":8080", "0.0.0.0:8080")
 * @return Result containing TCPListener or an error
 */
gocxx::base::Result<std::shared_ptr<TCPListener>> ListenTCP(
    const std::string& network,
    const std::string& address);

/**
 * @brief Convenience function to listen on a TCP address
 * 
 * @param address Address string (e.g., ":8080")
 * @return Result containing listener or an error
 */
gocxx::base::Result<std::shared_ptr<Listener>> Listen(const std::string& address);

} // namespace gocxx::net
