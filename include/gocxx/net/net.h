#pragma once

/**
 * @file net.h
 * @brief Core networking interfaces and types (Go-style)
 * 
 * Provides common networking abstractions:
 * - Addr: Network address interface
 * - Conn: Generic network connection interface
 * - Listener: Accepts incoming connections
 * - PacketConn: Packet-oriented network connection
 */

#include <string>
#include <memory>
#include <chrono>
#include <gocxx/base/result.h>
#include <gocxx/io/io.h>
#include <gocxx/context/context.h>
#include <gocxx/errors/errors.h>

namespace gocxx::net {

// Common network errors
extern std::shared_ptr<gocxx::errors::Error> ErrClosed;
extern std::shared_ptr<gocxx::errors::Error> ErrTimeout;
extern std::shared_ptr<gocxx::errors::Error> ErrInvalidAddr;

/**
 * @brief Network address interface
 * 
 * Represents a network endpoint address.
 */
class Addr {
public:
    virtual ~Addr() = default;
    
    /**
     * @brief Returns the name of the network (e.g., "tcp", "udp", "unix")
     */
    virtual std::string Network() const = 0;
    
    /**
     * @brief Returns the string form of the address
     */
    virtual std::string String() const = 0;
};

/**
 * @brief Generic network connection interface
 * 
 * Represents a generic network connection that can read, write, and close.
 * Similar to Go's net.Conn interface.
 */
class Conn : public gocxx::io::Reader, 
             public gocxx::io::Writer, 
             public gocxx::io::Closer {
public:
    virtual ~Conn() = default;
    
    /**
     * @brief Returns the local network address
     */
    virtual std::shared_ptr<Addr> LocalAddr() = 0;
    
    /**
     * @brief Returns the remote network address
     */
    virtual std::shared_ptr<Addr> RemoteAddr() = 0;
    
    /**
     * @brief Sets the read deadline
     * 
     * @param deadline Absolute time point for read timeout
     */
    virtual gocxx::base::Result<void> SetReadDeadline(
        std::chrono::system_clock::time_point deadline) = 0;
    
    /**
     * @brief Sets the write deadline
     * 
     * @param deadline Absolute time point for write timeout
     */
    virtual gocxx::base::Result<void> SetWriteDeadline(
        std::chrono::system_clock::time_point deadline) = 0;
    
    /**
     * @brief Sets both read and write deadlines
     * 
     * @param deadline Absolute time point for both timeouts
     */
    virtual gocxx::base::Result<void> SetDeadline(
        std::chrono::system_clock::time_point deadline) = 0;
};

/**
 * @brief Network listener interface
 * 
 * Accepts incoming network connections.
 * Similar to Go's net.Listener interface.
 */
class Listener {
public:
    virtual ~Listener() = default;
    
    /**
     * @brief Accepts the next incoming connection
     * 
     * @return Result containing the accepted connection or an error
     */
    virtual gocxx::base::Result<std::shared_ptr<Conn>> Accept() = 0;
    
    /**
     * @brief Closes the listener
     */
    virtual gocxx::base::Result<void> Close() = 0;
    
    /**
     * @brief Returns the listener's network address
     */
    virtual std::shared_ptr<Addr> Address() = 0;
};

/**
 * @brief Packet-oriented network connection interface
 * 
 * For connectionless protocols like UDP.
 * Similar to Go's net.PacketConn interface.
 */
class PacketConn : public gocxx::io::Closer {
public:
    virtual ~PacketConn() = default;
    
    /**
     * @brief Reads a packet from the connection
     * 
     * @param buffer Buffer to read into
     * @param size Size of the buffer
     * @param addr Output parameter for the sender's address
     * @return Result containing number of bytes read or an error
     */
    virtual gocxx::base::Result<std::size_t> ReadFrom(
        uint8_t* buffer, 
        std::size_t size,
        std::shared_ptr<Addr>& addr) = 0;
    
    /**
     * @brief Writes a packet to the specified address
     * 
     * @param buffer Buffer containing data to write
     * @param size Size of the data
     * @param addr Destination address
     * @return Result containing number of bytes written or an error
     */
    virtual gocxx::base::Result<std::size_t> WriteTo(
        const uint8_t* buffer,
        std::size_t size,
        std::shared_ptr<Addr> addr) = 0;
    
    /**
     * @brief Returns the local network address
     */
    virtual std::shared_ptr<Addr> LocalAddr() = 0;
    
    /**
     * @brief Sets the read deadline
     */
    virtual gocxx::base::Result<void> SetReadDeadline(
        std::chrono::system_clock::time_point deadline) = 0;
    
    /**
     * @brief Sets the write deadline
     */
    virtual gocxx::base::Result<void> SetWriteDeadline(
        std::chrono::system_clock::time_point deadline) = 0;
    
    /**
     * @brief Sets both read and write deadlines
     */
    virtual gocxx::base::Result<void> SetDeadline(
        std::chrono::system_clock::time_point deadline) = 0;
};

} // namespace gocxx::net
