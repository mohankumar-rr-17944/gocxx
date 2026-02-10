/**
 * @file tcp_example.cpp
 * @brief Example demonstrating gocxx TCP networking with echo server/client
 */

#include <gocxx/gocxx.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace gocxx::net;
using namespace std::chrono_literals;

// TCP Echo Server
void runEchoServer() {
    std::cout << "[Server] Starting TCP echo server on :8080..." << std::endl;
    
    // Listen on port 8080
    auto listener_result = Listen(":8080");
    if (listener_result.Failed()) {
        std::cerr << "[Server] Failed to listen: " << listener_result.err->error() << std::endl;
        return;
    }
    
    auto listener = listener_result.value;
    std::cout << "[Server] Listening on " << listener->Address()->String() << std::endl;
    
    // Accept one connection for this example
    std::cout << "[Server] Waiting for connection..." << std::endl;
    auto conn_result = listener->Accept();
    if (conn_result.Failed()) {
        std::cerr << "[Server] Failed to accept: " << conn_result.err->error() << std::endl;
        return;
    }
    
    auto conn = conn_result.value;
    std::cout << "[Server] Accepted connection from " << conn->RemoteAddr()->String() << std::endl;
    
    // Echo data back to client
    uint8_t buffer[1024];
    while (true) {
        auto read_result = conn->Read(buffer, sizeof(buffer));
        if (read_result.Failed() || read_result.value == 0) {
            std::cout << "[Server] Connection closed" << std::endl;
            break;
        }
        
        std::string received(reinterpret_cast<char*>(buffer), read_result.value);
        std::cout << "[Server] Received: " << received;
        
        // Echo back
        auto write_result = conn->Write(buffer, read_result.value);
        if (write_result.Failed()) {
            std::cerr << "[Server] Failed to write: " << write_result.err->error() << std::endl;
            break;
        }
    }
    
    conn->close();
    listener->Close();
}

// TCP Echo Client
void runEchoClient() {
    // Give server time to start
    std::this_thread::sleep_for(500ms);
    
    std::cout << "[Client] Connecting to localhost:8080..." << std::endl;
    
    auto conn_result = Dial("localhost:8080");
    if (conn_result.Failed()) {
        std::cerr << "[Client] Failed to connect: " << conn_result.err->error() << std::endl;
        return;
    }
    
    auto conn = conn_result.value;
    std::cout << "[Client] Connected to " << conn->RemoteAddr()->String() << std::endl;
    
    // Send messages
    const char* messages[] = {
        "Hello, TCP!\n",
        "This is a test message.\n",
        "Goodbye!\n"
    };
    
    for (const char* msg : messages) {
        std::cout << "[Client] Sending: " << msg;
        
        auto write_result = conn->Write(
            reinterpret_cast<const uint8_t*>(msg), 
            strlen(msg));
        
        if (write_result.Failed()) {
            std::cerr << "[Client] Failed to send: " << write_result.err->error() << std::endl;
            break;
        }
        
        // Read echo response
        uint8_t buffer[1024];
        auto read_result = conn->Read(buffer, sizeof(buffer));
        if (read_result.Failed()) {
            std::cerr << "[Client] Failed to receive: " << read_result.err->error() << std::endl;
            break;
        }
        
        std::string received(reinterpret_cast<char*>(buffer), read_result.value);
        std::cout << "[Client] Received echo: " << received;
        
        std::this_thread::sleep_for(100ms);
    }
    
    conn->close();
    std::cout << "[Client] Connection closed" << std::endl;
}

int main() {
    std::cout << "=== gocxx TCP Echo Server/Client Example ===" << std::endl;
    
    // Run server in a separate thread
    std::thread server_thread(runEchoServer);
    
    // Run client in main thread
    runEchoClient();
    
    // Wait for server to finish
    server_thread.join();
    
    std::cout << "\n=== TCP Example completed ===" << std::endl;
    return 0;
}
