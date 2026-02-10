# gocxx v0.2.0 Release Notes

**Release Date:** February 10, 2026

## 🎉 What's New in v0.2.0

This release adds **SSL/TLS support** to gocxx, enabling secure HTTPS connections for both client and server applications!

### 🔒 SSL/TLS Support

The major feature of this release is comprehensive SSL/TLS support using OpenSSL:

- **HTTPS Client**: `http::Get()` and `http::Post()` now support `https://` URLs
- **HTTPS Server**: New `ListenAndServeTLS()` function for secure servers
- **TLS Module**: Complete TLS/SSL wrapper classes (`TLSConn`, `TLSListener`)
- **Certificate Support**: Full support for server certificates, client certificates, and CA validation

### 🚀 Key Features

#### HTTPS Client
```cpp
// Just change http:// to https:// - it works automatically!
auto response = http::Get("https://api.example.com/data");
if (response.Ok()) {
    std::cout << "Secure response: " << response.value.body << std::endl;
}
```

#### HTTPS Server
```cpp
auto mux = std::make_shared<ServeMux>();
mux->HandleFunc("/", [](ResponseWriter& w, const Request& req) {
    w.Write("Secure connection!");
});

// Start HTTPS server with your certificates
http::ListenAndServeTLS(":8443", "server.crt", "server.key", mux);
```

#### Direct TLS Connections
```cpp
TLSConfig config;
config.cert_file = "client.crt";
config.key_file = "client.key";

auto conn = DialTLS("tcp", "secure.server.com:443", &config);
```

### 📦 What's Included

- **New Module**: `gocxx::net::tls` with full SSL/TLS support
- **Updated HTTP Module**: Seamless HTTPS support in existing functions
- **New Example**: `https_example.cpp` demonstrating secure connections
- **Documentation**: Updated README and API docs with HTTPS usage

### 🔧 Requirements

- **OpenSSL 3.0+** (now required dependency)
- C++17 compatible compiler
- CMake 3.15+

### 📝 Changes Since v0.1.0

- Added OpenSSL integration for SSL/TLS
- Extended `TCPConn` and `TCPListener` with `GetFD()` method
- Added `ListenAndServeTLS()` for HTTPS servers
- Updated `Get()` and `Post()` to support HTTPS URLs
- Created comprehensive HTTPS example
- Updated project version to 0.2.0

### 🐛 Bug Fixes

None in this release - pure feature addition!

### ⚠️ Breaking Changes

**None!** This is a backward-compatible release. All existing HTTP code continues to work without modification.

### 🎯 Migration Guide

No migration needed! To use HTTPS:
1. Ensure OpenSSL 3.0+ is installed on your system
2. Simply use `https://` URLs instead of `http://` in your client code
3. Use `ListenAndServeTLS()` instead of `ListenAndServe()` for servers

### 📚 Documentation

- Updated [README.md](README.md) with HTTPS examples
- Updated [CHANGELOG.md](CHANGELOG.md) with detailed changes
- See [examples/https_example.cpp](examples/https_example.cpp) for a complete working example

### 🙏 Acknowledgments

This release brings gocxx closer to feature parity with Go's standard library, particularly the `net/http` and `crypto/tls` packages.

---

## Installation

```bash
git clone https://github.com/mohankumar-rr-17944/gocxx.git
cd gocxx
git checkout v0.2.0

mkdir build && cd build
cmake .. -DGOCXX_ENABLE_TESTS=ON -DGOCXX_ENABLE_EXAMPLES=ON
cmake --build .
ctest  # Run tests
```

## Quick Start

```cpp
#include <gocxx/gocxx.h>
#include <iostream>

using namespace gocxx::net::http;

int main() {
    // HTTPS client - that's it!
    auto response = Get("https://www.example.com");
    if (response.Ok()) {
        std::cout << "Status: " << response.value.status_code << std::endl;
    }
    return 0;
}
```

---

**Full Changelog**: https://github.com/mohankumar-rr-17944/gocxx/blob/v0.2.0/CHANGELOG.md
