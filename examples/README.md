# gocxx Examples

This directory contains example programs demonstrating various features of the gocxx library.

## Available Examples

### 1. context_example
Demonstrates context usage for cancellation and timeouts:
- Basic timeout context
- Manual cancellation
- Context with values
- Context hierarchy
- Deadline context
- Context utilities

### 2. tcp_example
Shows TCP networking with an echo server/client:
- TCP server listening and accepting connections
- TCP client connecting to server
- Reading and writing data over TCP
- Bi-directional communication

### 3. udp_example
Demonstrates UDP networking:
- UDP server listening for packets
- UDP client sending packets
- Reading from and writing to UDP connections
- Connectionless communication

### 4. http_example
Shows HTTP server and client functionality:
- HTTP server with multiple endpoints
- Handler registration with ServeMux
- GET and POST request handling
- HTTP client making requests

## Building Examples

### Using CMake from the main project

From the gocxx root directory:

```bash
mkdir build && cd build
cmake .. -DGOCXX_ENABLE_EXAMPLES=ON
cmake --build .
```

The example executables will be in `build/examples/`.

### Building examples standalone

From the examples directory:

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Running Examples

After building, run any example:

```bash
# From build directory
./examples/context_example
./examples/tcp_example
./examples/udp_example
./examples/http_example
```

## Example Output

Each example prints informative messages showing what operations are being performed. The networking examples (TCP, UDP, HTTP) demonstrate client-server communication patterns.

## Requirements

- C++17 compatible compiler
- CMake 3.15 or later
- gocxx library (built automatically if not found)

## Notes

- The HTTP example server runs indefinitely - press Ctrl+C to exit
- The TCP and UDP examples coordinate server and client in the same process
- All examples demonstrate Go-style patterns and APIs adapted to C++
