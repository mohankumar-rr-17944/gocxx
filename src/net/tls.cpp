#include <gocxx/net/tls.h>
#include <gocxx/errors/errors.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#include <wincrypt.h>
#include <openssl/x509.h>
#pragma comment(lib, "crypt32.lib")
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

// Load system CA certificates into an SSL_CTX
static bool LoadSystemCACerts(SSL_CTX* ctx) {
#ifdef _WIN32
    // On Windows, load certificates from the system "ROOT" certificate store
    HCERTSTORE hStore = CertOpenSystemStoreA(0, "ROOT");
    if (!hStore) {
        return false;
    }

    X509_STORE* store = SSL_CTX_get_cert_store(ctx);
    PCCERT_CONTEXT pContext = nullptr;
    int added = 0;

    while ((pContext = CertEnumCertificatesInStore(hStore, pContext)) != nullptr) {
        const unsigned char* data = pContext->pbCertEncoded;
        X509* x509 = d2i_X509(nullptr, &data, pContext->cbCertEncoded);
        if (x509) {
            if (X509_STORE_add_cert(store, x509) == 1) {
                added++;
            }
            X509_free(x509);
        }
    }

    CertCloseStore(hStore, 0);
    return added > 0;
#else
    // On Linux/macOS, the default paths usually work
    return SSL_CTX_set_default_verify_paths(ctx) == 1;
#endif
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
                // Use system CA certificates
                LoadSystemCACerts(ctx);
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
        LoadSystemCACerts(ctx);
    }
    
    // Create SSL structure
    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        SSL_CTX_free(ctx);
        tcp_conn->close();
        return {nullptr, gocxx::errors::New("SSL_new failed: " + GetSSLError())};
    }
    
    // Set SNI hostname (required by most servers for certificate selection)
    // Extract hostname from address (strip port)
    std::string hostname = address;
    auto colon_pos = hostname.find(':');
    if (colon_pos != std::string::npos) {
        hostname = hostname.substr(0, colon_pos);
    }
    SSL_set_tlsext_host_name(ssl, hostname.c_str());
    
    // Enable hostname verification if peer verification is on
    if (!config || !config->insecure_skip_verify) {
        SSL_set1_host(ssl, hostname.c_str());
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
    
    // Release fd ownership from tcp_conn so it won't close the socket on destruction
    tcp_conn->ReleaseFD();
    
    // Create TLS connection (takes ownership of fd, ssl, ctx)
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
