# Release Guide for gocxx v0.1.0

## Current Status

✅ **Code Ready:** All features implemented and tested
✅ **Version Updated:** CMakeLists.txt updated to v0.1.0
✅ **Changelog Created:** CHANGELOG.md documents all changes
✅ **Tests Passing:** 100% test success rate
✅ **Tag Created Locally:** v0.1.0 tag created with release notes

## Next Steps (Manual)

### 1. Push the Release Tag

The tag `v0.1.0` has been created locally. To push it to GitHub:

```bash
git push origin v0.1.0
```

Or push all tags:

```bash
git push origin --tags
```

### 2. Create GitHub Release

After the tag is pushed, create a GitHub Release:

1. Go to https://github.com/mohankumar-rr-17944/gocxx/releases/new
2. Select tag: `v0.1.0`
3. Release title: `v0.1.0 - Net Module Implementation`
4. Copy the release notes from below or CHANGELOG.md

### Release Notes for GitHub

```markdown
# gocxx v0.1.0 - Net Module Implementation

This is the first major release of gocxx featuring complete networking support!

## 🚀 Major Features

### Net Module (Networking)
- **TCP Support**: Full client/server implementation with `Dial()`, `Listen()`, `Accept()`
- **UDP Support**: Complete packet I/O with `DialUDP()`, `ListenUDP()`
- **HTTP Support**: Go-style HTTP client (`Get()`, `Post()`) and server (`ServeMux`, `HandleFunc()`)

### CI/CD Pipeline
- Multi-platform builds: Ubuntu (GCC, Clang), Windows (MSVC)
- Automated testing with CTest
- Build status badge

### Examples
- `tcp_example.cpp` - TCP echo server/client
- `udp_example.cpp` - UDP packet send/receive
- `http_example.cpp` - HTTP server and client

## 🔧 Installation

```bash
git clone https://github.com/mohankumar-rr-17944/gocxx.git
cd gocxx
git checkout v0.1.0
mkdir build && cd build
cmake ..
cmake --build .
```

## 📝 Usage Example

```cpp
#include <gocxx/gocxx.h>

// TCP Echo Server
auto listener = gocxx::net::Listen(":8080");
auto conn = listener.value->Accept();
uint8_t buffer[1024];
auto n = conn.value->Read(buffer, sizeof(buffer));
conn.value->Write(buffer, n.value);

// HTTP Server
auto mux = std::make_shared<gocxx::net::http::ServeMux>();
mux->HandleFunc("/", [](auto& w, const auto& req) {
    w.Write("Hello, World!");
});
gocxx::net::http::ListenAndServe(":8080", mux);
```

## 🐛 Bug Fixes

- Windows MSVC compatibility with NOMINMAX macro
- Build system improvements
- Socket naming conflict resolution

## 📚 Documentation

- Doxygen comments for all APIs
- Comprehensive examples with README
- Updated website documentation

## 🙏 Acknowledgments

Thanks to all contributors and the Go community for inspiration!

## 📦 What's Next?

- Performance optimizations
- Additional protocol support
- More examples and tutorials

See [CHANGELOG.md](CHANGELOG.md) for detailed changes.
```

### 3. Merge the Branch (If using PR workflow)

If you're using a pull request workflow:

1. Create/update PR from `copilot/implement-net-module` to `main`
2. Review and approve
3. Merge to `main`
4. The tag will follow the merge

### 4. Verify Release

After publishing:

```bash
# Clone fresh copy
git clone https://github.com/mohankumar-rr-17944/gocxx.git
cd gocxx
git checkout v0.1.0

# Build and test
mkdir build && cd build
cmake .. -DGOCXX_ENABLE_TESTS=ON
cmake --build .
ctest --output-on-failure

# Try examples
cmake .. -DGOCXX_ENABLE_EXAMPLES=ON
cmake --build . --target examples
./examples/tcp_example
```

## Release Artifacts

The following files are included in this release:

- **Source Code**: Complete C++17 library
- **Headers**: include/gocxx/ directory
- **Examples**: examples/ directory with working demonstrations
- **Tests**: tests/ directory with comprehensive test suite
- **Documentation**: README.md, CHANGELOG.md, examples/README.md
- **Build System**: CMakeLists.txt with all options

## Platform Support

- ✅ Linux (GCC 8+, Clang 7+)
- ✅ Windows (MSVC 2019+)
- ✅ macOS (Clang)

## Dependencies

All dependencies are automatically managed by CMake:
- nlohmann/json (v3.11.3)
- Google Test (for testing)

---

**Release Date:** 2026-02-10
**Tag:** v0.1.0
**Commit:** ba76eb05607e14c743a4e69a9c19f1b28f93b66c
