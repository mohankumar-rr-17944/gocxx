#include <gocxx/net/tls.h>
#include <gocxx/errors/errors.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#endif

namespace gocxx::net {

// Global SSL initialization
static std::once_flag ssl_init_flag;

void InitializeSSL() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
}

void CleanupSSL() {
    EVP_cleanup();
    ERR_free_strings();
}

// Helper to get SSL error string
static std::string GetSSLError() {
    char buf[256];
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    return std::string(buf);
}

// TLSConn implementation
TLSConn::TLSConn(int fd, SSL* ssl, SSL_CTX* ctx)
    : TCPConn(fd), ssl_(ssl), ctx_(ctx) {
}

TLSConn::~TLSConn() {
    if (ssl_) {
        SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
    if (ctx_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
}

gocxx::base::Result<std::size_t> TLSConn::Read(uint8_t* buffer, std::size_t count) {
    if (!ssl_) {
        return {0, gocxx::errors::New("TLS connection closed")};
    }
    
    int n = SSL_read(ssl_, buffer, static_cast<int>(count));
    if (n > 0) {
        return {static_cast<std::size_t>(n), nullptr};
    } else if (n == 0) {
        // Connection closed
        return {0, nullptr};
    } else {
        int err = SSL_get_error(ssl_, n);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            // Would block - retry
            return {0, nullptr};
        }
        return {0, gocxx::errors::New("SSL_read failed: " + GetSSLError())};
    }
}

gocxx::base::Result<std::size_t> TLSConn::Write(const uint8_t* buffer, std::size_t count) {
    if (!ssl_) {
        return {0, gocxx::errors::New("TLS connection closed")};
    }
    
    int n = SSL_write(ssl_, buffer, static_cast<int>(count));
    if (n > 0) {
        return {static_cast<std::size_t>(n), nullptr};
    } else {
        int err = SSL_get_error(ssl_, n);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            // Would block - retry
            return {0, gocxx::errors::New("SSL_write would block")};
        }
        return {0, gocxx::errors::New("SSL_write failed: " + GetSSLError())};
    }
}

void TLSConn::close() {
    if (ssl_) {
        SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
    TCPConn::close();
}

// TLSListener implementation
TLSListener::TLSListener(int fd, SSL_CTX* ctx)
    : TCPListener(fd), ctx_(ctx) {
}

TLSListener::~TLSListener() {
    if (ctx_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
}

gocxx::base::Result<std::shared_ptr<Conn>> TLSListener::Accept() {
    // Accept TCP connection first
    auto result = TCPListener::Accept();
    if (result.Failed()) {
        return result;
    }
    
    auto tcp_conn = std::dynamic_pointer_cast<TCPConn>(result.value);
    if (!tcp_conn) {
        return {nullptr, gocxx::errors::New("failed to cast to TCPConn")};
    }
    
    // Wrap with SSL
    SSL* ssl = SSL_new(ctx_);
    if (!ssl) {
        tcp_conn->close();
        return {nullptr, gocxx::errors::New("SSL_new failed: " + GetSSLError())};
    }
    
    // Get the file descriptor from TCPConn
    int fd = tcp_conn->GetFD();
    SSL_set_fd(ssl, fd);
    
    // Perform SSL handshake
    int ret = SSL_accept(ssl);
    if (ret <= 0) {
        SSL_free(ssl);
        tcp_conn->close();
        return {nullptr, gocxx::errors::New("SSL_accept failed: " + GetSSLError())};
    }
    
    // Create TLS connection
    // Note: We need to release the fd from tcp_conn to avoid double-close
    auto tls_conn = std::make_shared<TLSConn>(fd, ssl, nullptr);
    
    return {tls_conn, nullptr};
}

gocxx::base::Result<void> TLSListener::Close() {
    return TCPListener::Close();
}

// DialTLS implementation
gocxx::base::Result<std::shared_ptr<TLSConn>> DialTLS(
    const std::string& network,
    const std::string& address,
    const TLSConfig* config) {
    
    // Initialize SSL once
    std::call_once(ssl_init_flag, InitializeSSL);
    
    // First, establish TCP connection
    auto tcp_result = DialTCP(network, address);
    if (tcp_result.Failed()) {
        return {nullptr, tcp_result.err};
    }
    
    auto tcp_conn = tcp_result.value;
    
    // Create SSL context
    const SSL_METHOD* method = TLS_client_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        tcp_conn->close();
        return {nullptr, gocxx::errors::New("SSL_CTX_new failed: " + GetSSLError())};
    }
    
    // Configure SSL context
    if (config) {
        if (config->insecure_skip_verify) {
            SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
        } else {
            SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
            
            // Load CA certificates if provided
            if (!config->ca_file.empty()) {
                if (!SSL_CTX_load_verify_locations(ctx, config->ca_file.c_str(), nullptr)) {
                    SSL_CTX_free(ctx);
                    tcp_conn->close();
                    return {nullptr, gocxx::errors::New("Failed to load CA file: " + GetSSLError())};
                }
            } else {
                // Use system default CA certificates
                SSL_CTX_set_default_verify_paths(ctx);
            }
        }
        
        // Load client certificate and key if provided
        if (!config->cert_file.empty() && !config->key_file.empty()) {
            if (SSL_CTX_use_certificate_file(ctx, config->cert_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
                SSL_CTX_free(ctx);
                tcp_conn->close();
                return {nullptr, gocxx::errors::New("Failed to load certificate: " + GetSSLError())};
            }
            if (SSL_CTX_use_PrivateKey_file(ctx, config->key_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
                SSL_CTX_free(ctx);
                tcp_conn->close();
                return {nullptr, gocxx::errors::New("Failed to load private key: " + GetSSLError())};
            }
        }
    } else {
        // Default: verify peer and use system CA certificates
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
        SSL_CTX_set_default_verify_paths(ctx);
    }
    
    // Create SSL structure
    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        SSL_CTX_free(ctx);
        tcp_conn->close();
        return {nullptr, gocxx::errors::New("SSL_new failed: " + GetSSLError())};
    }
    
    // Get the file descriptor
    int fd = tcp_conn->GetFD();
    SSL_set_fd(ssl, fd);
    
    // Perform SSL handshake
    int ret = SSL_connect(ssl);
    if (ret <= 0) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        tcp_conn->close();
        return {nullptr, gocxx::errors::New("SSL_connect failed: " + GetSSLError())};
    }
    
    // Create TLS connection
    auto tls_conn = std::make_shared<TLSConn>(fd, ssl, ctx);
    
    return {tls_conn, nullptr};
}

// ListenTLS implementation
gocxx::base::Result<std::shared_ptr<TLSListener>> ListenTLS(
    const std::string& network,
    const std::string& address,
    const TLSConfig& config) {
    
    // Initialize SSL once
    std::call_once(ssl_init_flag, InitializeSSL);
    
    // Validate that certificate and key are provided
    if (config.cert_file.empty() || config.key_file.empty()) {
        return {nullptr, gocxx::errors::New("certificate and key files are required for TLS server")};
    }
    
    // Create SSL context
    const SSL_METHOD* method = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        return {nullptr, gocxx::errors::New("SSL_CTX_new failed: " + GetSSLError())};
    }
    
    // Load certificate
    if (SSL_CTX_use_certificate_file(ctx, config.cert_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
        SSL_CTX_free(ctx);
        return {nullptr, gocxx::errors::New("Failed to load certificate: " + GetSSLError())};
    }
    
    // Load private key
    if (SSL_CTX_use_PrivateKey_file(ctx, config.key_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
        SSL_CTX_free(ctx);
        return {nullptr, gocxx::errors::New("Failed to load private key: " + GetSSLError())};
    }
    
    // Verify that the private key matches the certificate
    if (!SSL_CTX_check_private_key(ctx)) {
        SSL_CTX_free(ctx);
        return {nullptr, gocxx::errors::New("Private key does not match certificate")};
    }
    
    // Create TCP listener
    auto listener_result = ListenTCP(network, address);
    if (listener_result.Failed()) {
        SSL_CTX_free(ctx);
        return {nullptr, listener_result.err};
    }
    
    auto tcp_listener = listener_result.value;
    
    // Create TLS listener
    int fd = tcp_listener->GetFD();
    auto tls_listener = std::make_shared<TLSListener>(fd, ctx);
    
    return {tls_listener, nullptr};
}

} // namespace gocxx::net
