#pragma once

/**
 * @file udp.h
 * @brief UDP networking support (Go-style)
 * 
 * Provides UDP client and server functionality:
 * - UDPAddr: UDP network address
 * - UDPConn: UDP connection
 * - DialUDP, ListenUDP: Connection establishment
 */

#include <gocxx/net/net.h>
#include <string>
#include <memory>

namespace gocxx::net {

/**
 * @brief UDP network address
 */
class UDPAddr : public Addr {
public:
    std::string ip;    ///< IP address
    int port;          ///< Port number
    
    UDPAddr() : port(0) {}
    UDPAddr(const std::string& ip, int port) : ip(ip), port(port) {}
    
    std::string Network() const override { return "udp"; }
    std::string String() const override;
};

/**
 * @brief UDP connection
 * 
 * Implements a UDP network connection for packet-based communication.
 */
class UDPConn : public PacketConn {
public:
    UDPConn(int socket_fd, std::shared_ptr<UDPAddr> local_addr);
    virtual ~UDPConn();
    
    // PacketConn interface
    gocxx::base::Result<std::size_t> ReadFrom(
        uint8_t* buffer,
        std::size_t size,
        std::shared_ptr<Addr>& addr) override;
    
    gocxx::base::Result<std::size_t> WriteTo(
        const uint8_t* buffer,
        std::size_t size,
        std::shared_ptr<Addr> addr) override;
    
    std::shared_ptr<Addr> LocalAddr() override;
    
    gocxx::base::Result<void> SetReadDeadline(
        std::chrono::system_clock::time_point deadline) override;
    gocxx::base::Result<void> SetWriteDeadline(
        std::chrono::system_clock::time_point deadline) override;
    gocxx::base::Result<void> SetDeadline(
        std::chrono::system_clock::time_point deadline) override;
    
    // io::Closer interface
    void close() override;
    
    /**
     * @brief Reads from a connected UDP socket
     * 
     * @param buffer Buffer to read into
     * @param size Size of the buffer
     * @return Result containing number of bytes read or an error
     */
    gocxx::base::Result<std::size_t> Read(uint8_t* buffer, std::size_t size);
    
    /**
     * @brief Writes to a connected UDP socket
     * 
     * @param buffer Buffer containing data to write
     * @param size Size of the data
     * @return Result containing number of bytes written or an error
     */
    gocxx::base::Result<std::size_t> Write(const uint8_t* buffer, std::size_t size);
    
    /**
     * @brief Reads from a specific address (UDP-specific)
     * 
     * @param buffer Buffer to read into
     * @param size Size of the buffer
     * @param addr Output parameter for the sender's address
     * @return Result containing number of bytes read or an error
     */
    gocxx::base::Result<std::size_t> ReadFromUDP(
        uint8_t* buffer,
        std::size_t size,
        std::shared_ptr<UDPAddr>& addr);
    
    /**
     * @brief Writes to a specific address (UDP-specific)
     * 
     * @param buffer Buffer containing data to write
     * @param size Size of the data
     * @param addr Destination address
     * @return Result containing number of bytes written or an error
     */
    gocxx::base::Result<std::size_t> WriteToUDP(
        const uint8_t* buffer,
        std::size_t size,
        std::shared_ptr<UDPAddr> addr);

private:
    int socket_fd_;
    std::shared_ptr<UDPAddr> local_addr_;
    bool closed_;
    
    std::chrono::system_clock::time_point read_deadline_;
    std::chrono::system_clock::time_point write_deadline_;
};

/**
 * @brief Resolves a UDP address from a string
 * 
 * @param network Network type ("udp", "udp4", "udp6")
 * @param address Address string (e.g., "localhost:8080", "192.168.1.1:80")
 * @return Result containing UDPAddr or an error
 */
gocxx::base::Result<std::shared_ptr<UDPAddr>> ResolveUDPAddr(
    const std::string& network,
    const std::string& address);

/**
 * @brief Dials a UDP connection to the specified address
 * 
 * Similar to Go's net.DialUDP("udp", nil, raddr)
 * 
 * @param network Network type ("udp", "udp4", "udp6")
 * @param local_addr Local address (can be nullptr for automatic selection)
 * @param remote_addr Remote address
 * @return Result containing UDPConn or an error
 */
gocxx::base::Result<std::shared_ptr<UDPConn>> DialUDP(
    const std::string& network,
    std::shared_ptr<UDPAddr> local_addr,
    std::shared_ptr<UDPAddr> remote_addr);

/**
 * @brief Listens for incoming UDP packets on the specified address
 * 
 * Similar to Go's net.ListenUDP("udp", laddr)
 * 
 * @param network Network type ("udp", "udp4", "udp6")
 * @param local_addr Local address to bind to
 * @return Result containing UDPConn or an error
 */
gocxx::base::Result<std::shared_ptr<UDPConn>> ListenUDP(
    const std::string& network,
    std::shared_ptr<UDPAddr> local_addr);

/**
 * @brief Convenience function to listen on a UDP address
 * 
 * @param address Address string (e.g., ":8080")
 * @return Result containing UDPConn or an error
 */
gocxx::base::Result<std::shared_ptr<UDPConn>> ListenUDPSimple(const std::string& address);

} // namespace gocxx::net
