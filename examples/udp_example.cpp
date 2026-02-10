/**
 * @file udp_example.cpp
 * @brief Example demonstrating gocxx UDP networking
 */

#include <gocxx/gocxx.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace gocxx::net;
using namespace std::chrono_literals;

// UDP Server
void runUDPServer() {
    std::cout << "[Server] Starting UDP server on :9090..." << std::endl;
    
    auto server_result = ListenUDPSimple(":9090");
    if (server_result.Failed()) {
        std::cerr << "[Server] Failed to listen: " << server_result.err->error() << std::endl;
        return;
    }
    
    auto server = server_result.value;
    std::cout << "[Server] Listening on " << server->LocalAddr()->String() << std::endl;
    
    // Receive and echo back 3 messages
    for (int i = 0; i < 3; ++i) {
        uint8_t buffer[1024];
        std::shared_ptr<Addr> sender_addr;
        
        auto read_result = server->ReadFrom(buffer, sizeof(buffer), sender_addr);
        if (read_result.Failed()) {
            std::cerr << "[Server] Failed to receive: " << read_result.err->error() << std::endl;
            break;
        }
        
        std::string received(reinterpret_cast<char*>(buffer), read_result.value);
        std::cout << "[Server] Received from " << sender_addr->String() << ": " << received;
        
        // Echo back to sender
        auto write_result = server->WriteTo(buffer, read_result.value, sender_addr);
        if (write_result.Failed()) {
            std::cerr << "[Server] Failed to send: " << write_result.err->error() << std::endl;
            break;
        }
        
        std::cout << "[Server] Echoed back to " << sender_addr->String() << std::endl;
    }
    
    server->close();
    std::cout << "[Server] Server closed" << std::endl;
}

// UDP Client
void runUDPClient() {
    // Give server time to start
    std::this_thread::sleep_for(500ms);
    
    std::cout << "[Client] Creating UDP client..." << std::endl;
    
    // Resolve server address
    auto addr_result = ResolveUDPAddr("udp", "localhost:9090");
    if (addr_result.Failed()) {
        std::cerr << "[Client] Failed to resolve address: " << addr_result.err->error() << std::endl;
        return;
    }
    
    auto server_addr = addr_result.value;
    
    // Create UDP connection
    auto client_result = DialUDP("udp", nullptr, server_addr);
    if (client_result.Failed()) {
        std::cerr << "[Client] Failed to create connection: " << client_result.err->error() << std::endl;
        return;
    }
    
    auto client = client_result.value;
    std::cout << "[Client] Local address: " << client->LocalAddr()->String() << std::endl;
    
    // Send messages
    const char* messages[] = {
        "Hello, UDP!\n",
        "Message number 2\n",
        "Final message\n"
    };
    
    for (const char* msg : messages) {
        std::cout << "[Client] Sending: " << msg;
        
        auto write_result = client->Write(
            reinterpret_cast<const uint8_t*>(msg),
            strlen(msg));
        
        if (write_result.Failed()) {
            std::cerr << "[Client] Failed to send: " << write_result.err->error() << std::endl;
            break;
        }
        
        // Receive echo
        uint8_t buffer[1024];
        auto read_result = client->Read(buffer, sizeof(buffer));
        if (read_result.Failed()) {
            std::cerr << "[Client] Failed to receive: " << read_result.err->error() << std::endl;
            break;
        }
        
        std::string received(reinterpret_cast<char*>(buffer), read_result.value);
        std::cout << "[Client] Received echo: " << received;
        
        std::this_thread::sleep_for(100ms);
    }
    
    client->close();
    std::cout << "[Client] Client closed" << std::endl;
}

int main() {
    std::cout << "=== gocxx UDP Example ===" << std::endl;
    
    // Run server in a separate thread
    std::thread server_thread(runUDPServer);
    
    // Run client in main thread
    runUDPClient();
    
    // Wait for server to finish
    server_thread.join();
    
    std::cout << "\n=== UDP Example completed ===" << std::endl;
    return 0;
}
