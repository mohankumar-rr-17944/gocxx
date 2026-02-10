# Changelog

All notable changes to the gocxx project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] - 2026-02-10

### Added

#### TLS/SSL Support (HTTPS)
- **OpenSSL Integration**: Added OpenSSL as a dependency for SSL/TLS support
- **TLS Module**: New `gocxx::net::tls` module for secure connections
  - `TLSConn` class for TLS-encrypted TCP connections
  - `TLSListener` class for accepting secure connections
  - `TLSConfig` struct for configuring SSL/TLS settings
  - `DialTLS()` function for creating secure client connections
  - `ListenTLS()` function for creating secure servers
  
- **HTTPS Client Support**: Extended HTTP client functions
  - `http::Get()` now supports both `http://` and `https://` URLs
  - `http::Post()` now supports both `http://` and `https://` URLs
  - Automatic TLS connection establishment for HTTPS URLs
  - Certificate verification using system CA certificates by default
  
- **HTTPS Server Support**:
  - `http::ListenAndServeTLS()` function for HTTPS servers
  - Full TLS handshake and encryption for server connections
  - Certificate and private key configuration
  
- **Examples**:
  - `https_example.cpp` demonstrating HTTPS server and client
  - Self-signed certificates for testing (`server.crt`, `server.key`)
  
#### API Enhancements
- Added `GetFD()` method to `TCPConn` and `TCPListener` for TLS wrapping
- Added protected constructors to `TCPConn` and `TCPListener` for inheritance

### Technical Details
- SSL/TLS implementation uses OpenSSL 3.0+
- Certificate verification enabled by default (can be disabled for testing)
- Support for custom CA certificates
- Support for client certificates (mutual TLS)
- All TLS operations integrate seamlessly with existing `io::Reader`, `io::Writer`, and `io::Closer` interfaces

## [0.1.0] - 2026-02-10

### Added

#### Net Module (Networking)
- **TCP Support**: Full TCP client/server implementation
  - `TCPAddr`, `TCPConn`, `TCPListener` classes
  - `Dial()` for client connections
  - `Listen()` and `Accept()` for servers
  - Platform-specific socket code (Winsock2 on Windows, POSIX on Linux/macOS)
  
- **UDP Support**: Complete UDP networking
  - `UDPAddr`, `UDPConn` classes
  - `DialUDP()` and `ListenUDP()` functions
  - `ReadFrom()` and `WriteTo()` for packet I/O
  
- **HTTP Support**: Go-style HTTP client and server
  - `http::Get()` and `http::Post()` client functions
  - `http::Server`, `http::ServeMux` for routing
  - `http::HandleFunc()` for handler registration
  - `http::Request`, `http::Response`, `http::ResponseWriter`
  - `http::ListenAndServe()` for quick server setup

#### CI/CD Pipeline
- GitHub Actions workflow for continuous integration
- Multi-platform builds: Ubuntu (GCC, Clang), Windows (MSVC)
- Automated testing with CTest
- CMake dependency caching for faster builds
- Build status badge in README

#### Examples
- `tcp_example.cpp` - TCP echo server/client demonstration
- `udp_example.cpp` - UDP packet send/receive example
- `http_example.cpp` - HTTP server with multiple endpoints and client
- CMake support for building examples (`-DGOCXX_ENABLE_EXAMPLES=ON`)
- Comprehensive README for examples directory

#### Documentation
- Doxygen comments for all net module APIs
- Updated website to show implemented modules
- Examples README with build instructions

### Fixed
- **Windows MSVC Compatibility**: Added `NOMINMAX` macro before Windows headers to prevent min/max macro conflicts with STL
- **Build Issues**: Fixed missing `#include <condition_variable>` in io.cpp
- **Duration Constants**: Added proper static const definitions in duration.cpp
- **Result Type Usage**: Corrected Result<void> construction in os.cpp
- **Socket Naming**: Renamed close macros to SOCKET_CLOSE to avoid POSIX conflicts

### Changed
- Updated JSON module status from "Planned" to "Implemented" in website
- Moved CI/CD from planned to completed in roadmap
- Enhanced documentation for all modules

### Technical Details
- All network operations return `gocxx::base::Result<T>` for error handling
- Network connections implement `io::Reader`, `io::Writer`, `io::Closer` interfaces
- Cross-platform socket abstraction using `#ifdef _WIN32` guards
- Thread-based HTTP request dispatching
- Comprehensive test coverage for net module

## [0.0.1] - Initial Development

### Added
- Core modules: base, sync, time, io, os, errors, context
- Channel implementation with select support
- Go-style defer mechanism
- Result types for error handling
- JSON support via nlohmann/json integration
- Basic test infrastructure

---

[0.1.0]: https://github.com/mohankumar-rr-17944/gocxx/releases/tag/v0.1.0
