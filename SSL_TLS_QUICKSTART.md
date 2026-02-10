# Quick Start Guide: SSL/TLS & HTTPS in gocxx

This guide helps you quickly get started with SSL/TLS and HTTPS support in gocxx.

## Prerequisites

1. **Install OpenSSL** (if not already present):

```bash
# Ubuntu/Debian
sudo apt-get install libssl-dev

# macOS
brew install openssl

# Windows
# Download from https://slproweb.com/products/Win32OpenSSL.html
```

2. **Build gocxx** with SSL support:

```bash
git clone https://github.com/mohankumar-rr-17944/gocxx.git
cd gocxx
mkdir build && cd build
cmake .. -DGOCXX_ENABLE_EXAMPLES=ON
cmake --build .
```

## HTTPS Client in 3 Lines

Making HTTPS requests is as simple as using `https://` URLs:

```cpp
#include <gocxx/gocxx.h>
#include <iostream>

int main() {
    // Just use https:// - it works automatically!
    auto response = gocxx::net::http::Get("https://www.example.com");
    
    if (response.Ok()) {
        std::cout << "Status: " << response.value.status_code << std::endl;
        std::cout << "Body: " << response.value.body << std::endl;
    } else {
        std::cerr << "Error: " << response.err->error() << std::endl;
    }
    
    return 0;
}
```

**That's it!** Certificate verification is automatic using system CA certificates.

## HTTPS Server in 5 Minutes

### Step 1: Generate Certificates (for testing)

```bash
# Generate self-signed certificate
openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt \
    -days 365 -nodes -subj "/CN=localhost"
```

### Step 2: Create HTTPS Server

```cpp
#include <gocxx/gocxx.h>
#include <iostream>

using namespace gocxx::net::http;

int main() {
    auto mux = std::make_shared<ServeMux>();
    
    // Add your handlers
    mux->HandleFunc("/", [](ResponseWriter& w, const Request& req) {
        w.Header()["content-type"] = "text/html";
        w.Write("<h1>Secure HTTPS Server!</h1>");
    });
    
    // Start HTTPS server - just add certificate files!
    std::cout << "Starting HTTPS server on :8443..." << std::endl;
    auto result = ListenAndServeTLS(":8443", "server.crt", "server.key", mux);
    
    if (result.Failed()) {
        std::cerr << "Server error: " << result.err->error() << std::endl;
        return 1;
    }
    
    return 0;
}
```

### Step 3: Test Your Server

```bash
# Accept self-signed certificate with -k flag
curl -k https://localhost:8443/
```

## Advanced: Custom TLS Configuration

For more control over TLS, use the TLS module directly:

```cpp
#include <gocxx/net/tls.h>
using namespace gocxx::net;

// Configure TLS with custom settings
TLSConfig config;
config.cert_file = "client.crt";           // Client certificate
config.key_file = "client.key";            // Client private key
config.ca_file = "/path/to/custom/ca.crt"; // Custom CA
config.insecure_skip_verify = false;       // Verify certificates

// Create secure connection
auto conn = DialTLS("tcp", "secure.server.com:443", &config);

if (conn.Ok()) {
    // Use connection like any TCPConn
    std::string msg = "Hello via TLS!";
    conn.value->Write((const uint8_t*)msg.c_str(), msg.length());
    
    uint8_t buffer[1024];
    auto result = conn.value->Read(buffer, sizeof(buffer));
}
```

## Common Use Cases

### 1. REST API Client (HTTPS)

```cpp
// GET request
auto response = http::Get("https://api.example.com/users");

// POST request
auto post_resp = http::Post(
    "https://api.example.com/users",
    "application/json",
    R"({"name": "John", "email": "john@example.com"})"
);
```

### 2. Microservice with HTTPS

```cpp
auto mux = std::make_shared<ServeMux>();

mux->HandleFunc("/health", [](ResponseWriter& w, const Request& req) {
    w.Header()["content-type"] = "application/json";
    w.Write(R"({"status": "healthy"})");
});

mux->HandleFunc("/api/data", [](ResponseWriter& w, const Request& req) {
    // Your API logic here
    w.Write("API response");
});

// Production HTTPS server
ListenAndServeTLS(":8443", "server.crt", "server.key", mux);
```

### 3. Secure TCP Server

```cpp
TLSConfig config;
config.cert_file = "server.crt";
config.key_file = "server.key";

auto listener = ListenTLS("tcp", ":9443", config);

while (true) {
    auto conn = listener.value->Accept();
    if (conn.Ok()) {
        // Handle secure connection
        std::thread([conn = conn.value]() {
            uint8_t buffer[4096];
            auto n = conn->Read(buffer, sizeof(buffer));
            // Process data...
            conn->close();
        }).detach();
    }
}
```

## Production Certificates

For production use, get certificates from a trusted CA:

1. **Let's Encrypt** (Free): https://letsencrypt.org/
2. **Commercial CAs**: DigiCert, Sectigo, etc.
3. **Self-managed PKI**: For internal services

Place your production certificates where your application can read them:

```cpp
// Use environment variables for flexibility
const char* cert = std::getenv("TLS_CERT_FILE");
const char* key = std::getenv("TLS_KEY_FILE");

ListenAndServeTLS(":443", cert ? cert : "server.crt", 
                           key ? key : "server.key", mux);
```

## Troubleshooting

### "OpenSSL not found" during build

```bash
# Make sure OpenSSL development files are installed
# Ubuntu/Debian:
sudo apt-get install libssl-dev

# macOS:
brew install openssl
# You may need to tell CMake where to find it:
cmake .. -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl
```

### "Certificate verification failed"

For testing with self-signed certificates:

```cpp
TLSConfig config;
config.insecure_skip_verify = true;  // Only for testing!

auto conn = DialTLS("tcp", "localhost:8443", &config);
```

**Note:** Never use `insecure_skip_verify = true` in production!

### "SSL_connect failed"

Common causes:
1. Wrong port (use 443 for HTTPS, not 80)
2. Server doesn't support TLS
3. Certificate/key mismatch

Check server logs and verify your certificates match.

## Examples

See the `examples/` directory:
- `https_example.cpp` - Complete HTTPS server and client
- `http_example.cpp` - HTTP server (can be upgraded to HTTPS)

Build and run:

```bash
cd build
cmake .. -DGOCXX_ENABLE_EXAMPLES=ON
cmake --build .
cd examples
./https_example
```

## Next Steps

- Read the [full documentation](README.md)
- Check the [API reference](docs/)
- See [examples](examples/) for more use cases
- Review [CHANGELOG.md](CHANGELOG.md) for version history

## Need Help?

- 📖 Documentation: [README.md](README.md)
- 🐛 Issues: https://github.com/mohankumar-rr-17944/gocxx/issues
- 💬 Discussions: https://github.com/mohankumar-rr-17944/gocxx/discussions

---

**Happy secure coding with gocxx!** 🔒
