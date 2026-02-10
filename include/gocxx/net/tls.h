#pragma once

/**
 * @file tls.h
 * @brief TLS/SSL support for secure connections (using OpenSSL)
 * 
 * Provides TLS/SSL functionality:
 * - Secure TCP connections
 * - Certificate support
 * - TLS client and server
 */

#include <gocxx/net/tcp.h>
#include <memory>
#include <string>

// Forward declare OpenSSL types to avoid exposing them in the header
typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;

namespace gocxx::net {

/**
 * @brief TLS configuration
 */
struct TLSConfig {
    std::string cert_file;      ///< Path to certificate file (PEM format)
    std::string key_file;       ///< Path to private key file (PEM format)
    std::string ca_file;        ///< Path to CA certificate file for verification
    bool insecure_skip_verify;  ///< Skip certificate verification (insecure, for testing only)
    
    TLSConfig() : insecure_skip_verify(false) {}
};

/**
 * @brief TLS/SSL connection wrapper
 * 
 * Wraps a TCP connection with TLS encryption
 */
class TLSConn : public TCPConn {
public:
    TLSConn(int fd, SSL* ssl, SSL_CTX* ctx);
    virtual ~TLSConn();
    
    // Override TCPConn methods to use SSL
    gocxx::base::Result<std::size_t> Read(uint8_t* buffer, std::size_t count) override;
    gocxx::base::Result<std::size_t> Write(const uint8_t* buffer, std::size_t count) override;
    void close() override;
    
private:
    SSL* ssl_;
    SSL_CTX* ctx_;
};

/**
 * @brief TLS listener for accepting secure connections
 */
class TLSListener : public TCPListener {
public:
    TLSListener(int fd, SSL_CTX* ctx);
    virtual ~TLSListener();
    
    // Override Accept to return TLS connections
    gocxx::base::Result<std::shared_ptr<Conn>> Accept() override;
    gocxx::base::Result<void> Close() override;
    
private:
    SSL_CTX* ctx_;
};

/**
 * @brief Dials a TLS connection to the given address
 * 
 * Similar to Go's tls.Dial(network, address)
 * 
 * @param network Network type (must be "tcp")
 * @param address Address to connect to (host:port)
 * @param config TLS configuration (optional)
 * @return Result containing TLS connection or an error
 */
gocxx::base::Result<std::shared_ptr<TLSConn>> DialTLS(
    const std::string& network,
    const std::string& address,
    const TLSConfig* config = nullptr);

/**
 * @brief Listens for TLS connections on the given address
 * 
 * Similar to Go's tls.Listen(network, address, config)
 * 
 * @param network Network type (must be "tcp")
 * @param address Address to listen on (e.g., ":8443")
 * @param config TLS configuration with certificate and key
 * @return Result containing TLS listener or an error
 */
gocxx::base::Result<std::shared_ptr<TLSListener>> ListenTLS(
    const std::string& network,
    const std::string& address,
    const TLSConfig& config);

/**
 * @brief Initialize OpenSSL library (called automatically)
 */
void InitializeSSL();

/**
 * @brief Cleanup OpenSSL library (called automatically)
 */
void CleanupSSL();

} // namespace gocxx::net
