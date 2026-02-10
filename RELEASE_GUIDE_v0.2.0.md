# Release Guide for gocxx v0.2.0

## Current Status

✅ **Code Ready:** SSL/TLS support implemented and tested
✅ **Version Updated:** CMakeLists.txt updated to v0.2.0
✅ **Changelog Created:** CHANGELOG.md documents all changes
✅ **Tests Passing:** 100% test success rate
✅ **Documentation Updated:** README.md includes HTTPS examples
✅ **Release Notes Created:** RELEASE_NOTES_v0.2.0.md

## What's New in v0.2.0

This release adds **SSL/TLS support using OpenSSL**, enabling secure HTTPS connections for both client and server applications.

### Major Features
- HTTPS client support (`http::Get()` and `http::Post()` with `https://` URLs)
- HTTPS server support (`ListenAndServeTLS()`)
- TLS module with `TLSConn` and `TLSListener`
- Complete certificate support (server, client, CA)
- New HTTPS example (`examples/https_example.cpp`)

## Creating the Release

### Step 1: Verify Everything Works

```bash
# Build with tests
cd /path/to/gocxx
mkdir -p build && cd build
cmake .. -DGOCXX_ENABLE_TESTS=ON -DGOCXX_ENABLE_EXAMPLES=ON
cmake --build .

# Run tests
ctest --output-on-failure

# Test examples
./examples/https_example
```

### Step 2: Create and Push Tag

```bash
# Create annotated tag with release notes
git tag -a v0.2.0 -m "Release v0.2.0 - SSL/TLS Support

This release adds SSL/TLS support using OpenSSL for HTTPS functionality.

Major Features:
- HTTPS client (Get/Post with https:// URLs)
- HTTPS server (ListenAndServeTLS)
- TLS module (TLSConn, TLSListener)
- Certificate support
- HTTPS example

See CHANGELOG.md for detailed changes."

# Push the tag
git push origin v0.2.0
```

Or if using a branch workflow:

```bash
# Make sure branch is merged to main first
git checkout main
git merge copilot/add-ssl-support-openssl
git tag -a v0.2.0 -m "Release v0.2.0 - SSL/TLS Support"
git push origin main
git push origin v0.2.0
```

### Step 3: Create GitHub Release

1. Go to https://github.com/mohankumar-rr-17944/gocxx/releases/new
2. Select tag: `v0.2.0`
3. Release title: `v0.2.0 - SSL/TLS Support`
4. Copy release notes from `RELEASE_NOTES_v0.2.0.md`
5. Attach any additional release artifacts (optional)
6. Click "Publish release"

### Release Notes for GitHub

Use the content from `RELEASE_NOTES_v0.2.0.md`, which includes:

- Overview of SSL/TLS support
- Key features (HTTPS client, HTTPS server, TLS module)
- Code examples
- Installation instructions
- Migration guide (none needed - backward compatible!)
- Links to documentation

## Verification Steps

After publishing the release:

```bash
# Test fresh installation
git clone https://github.com/mohankumar-rr-17944/gocxx.git
cd gocxx
git checkout v0.2.0

# Verify OpenSSL is found
mkdir build && cd build
cmake .. -DGOCXX_ENABLE_TESTS=ON
# Should see: "Found OpenSSL: ... (found version "3.0.13")"

# Build and test
cmake --build .
ctest --output-on-failure

# Test HTTPS example
cmake .. -DGOCXX_ENABLE_EXAMPLES=ON
cmake --build .
cd examples
./https_example
```

## Release Checklist

- [x] All tests passing
- [x] Version bumped to 0.2.0 in CMakeLists.txt
- [x] CHANGELOG.md updated with v0.2.0 section
- [x] README.md updated with HTTPS examples
- [x] Release notes created (RELEASE_NOTES_v0.2.0.md)
- [x] Examples built and tested
- [x] Documentation complete
- [ ] Tag created and pushed
- [ ] GitHub release published
- [ ] Release announcement (optional)

## Platform Support

Tested and verified on:
- ✅ Linux (Ubuntu 22.04+, GCC 13+)
- ✅ Requires OpenSSL 3.0+
- ⚠️ Windows: Ensure OpenSSL is installed
- ⚠️ macOS: Ensure OpenSSL is installed via Homebrew

## Dependencies

- C++17 compiler
- CMake 3.15+
- **OpenSSL 3.0+** (NEW requirement)
- nlohmann/json (auto-fetched)
- Google Test (for testing, auto-fetched)

## Breaking Changes

**None!** This is a fully backward-compatible release. All existing HTTP code continues to work.

## Migration Guide for Users

To use the new HTTPS features:

1. **Install OpenSSL** (if not already present):
   ```bash
   # Ubuntu/Debian
   sudo apt-get install libssl-dev
   
   # macOS
   brew install openssl
   
   # Windows
   # Download from https://slproweb.com/products/Win32OpenSSL.html
   ```

2. **Update your code** (optional - HTTP code still works):
   ```cpp
   // Before (HTTP only)
   auto response = http::Get("http://api.example.com/data");
   
   // After (HTTPS support)
   auto response = http::Get("https://api.example.com/data");  // Just change URL!
   ```

3. **For HTTPS servers**:
   ```cpp
   // Generate certificates (self-signed for testing)
   openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt \
     -days 365 -nodes -subj "/CN=localhost"
   
   // Use ListenAndServeTLS instead of ListenAndServe
   auto mux = std::make_shared<ServeMux>();
   mux->HandleFunc("/", handler);
   http::ListenAndServeTLS(":8443", "server.crt", "server.key", mux);
   ```

## What's Next?

Future releases may include:
- HTTP/2 support
- WebSocket support
- Additional protocol support
- Performance optimizations
- More examples

---

**Release Date:** 2026-02-10
**Tag:** v0.2.0
**Previous Release:** v0.1.0

For detailed changes, see [CHANGELOG.md](CHANGELOG.md)
For release notes, see [RELEASE_NOTES_v0.2.0.md](RELEASE_NOTES_v0.2.0.md)
