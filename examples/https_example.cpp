/**
 * @file https_example.cpp
 * @brief Example demonstrating gocxx HTTPS server and client with SSL/TLS support
 */

#include <gocxx/gocxx.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace gocxx::net::http;
using namespace std::chrono_literals;

// HTTPS Server
void runHTTPSServer() {
    std::cout << "[Server] Starting HTTPS server on :8443..." << std::endl;
    
    auto mux = std::make_shared<ServeMux>();
    
    // Handler for root path
    mux->HandleFunc("/", [](ResponseWriter& w, const Request& req) {
        std::cout << "[Server] Handling request: " << req.method << " " << req.url << std::endl;
        w.Header()["content-type"] = "text/html";
        w.Write("<html><body><h1>Welcome to gocxx HTTPS Server!</h1>");
        w.Write("<p>This is a secure connection using TLS/SSL.</p>");
        w.Write("<p>Available endpoints:</p>");
        w.Write("<ul>");
        w.Write("<li><a href='/hello'>/hello</a> - Simple greeting</li>");
        w.Write("<li><a href='/secure'>/secure</a> - Secure data endpoint</li>");
        w.Write("</ul></body></html>");
    });
    
    // Handler for /hello
    mux->HandleFunc("/hello", [](ResponseWriter& w, const Request& req) {
        std::cout << "[Server] Handling /hello request" << std::endl;
        w.Header()["content-type"] = "text/plain";
        w.Write("Hello from gocxx HTTPS server!\nConnection is encrypted with TLS.\n");
    });
    
    // Handler for /secure
    mux->HandleFunc("/secure", [](ResponseWriter& w, const Request& req) {
        std::cout << "[Server] Handling /secure request" << std::endl;
        w.Header()["content-type"] = "application/json";
        w.Write("{\n");
        w.Write("  \"status\": \"secure\",\n");
        w.Write("  \"message\": \"This data is transmitted over TLS\",\n");
        w.Write("  \"encryption\": \"enabled\"\n");
        w.Write("}\n");
    });
    
    // Start HTTPS server (this blocks)
    // Note: Using self-signed certificates for demo purposes
    auto result = ListenAndServeTLS(":8443", "server.crt", "server.key", mux);
    if (result.Failed()) {
        std::cerr << "[Server] Server error: " << result.err->error() << std::endl;
    }
}

// HTTPS Client
void runHTTPSClient() {
    // Give server time to start
    std::this_thread::sleep_for(1s);
    
    std::cout << "[Client] Making HTTPS requests..." << std::endl;
    
    // Note: For self-signed certificates, we would need to configure
    // TLSConfig with insecure_skip_verify or provide the CA certificate.
    // For this example, we'll use HTTP for client testing since the
    // self-signed cert won't be trusted by default.
    
    std::cout << "\n=== HTTPS Client Example ===" << std::endl;
    std::cout << "To test HTTPS with curl (accepting self-signed cert):" << std::endl;
    std::cout << "  curl -k https://localhost:8443/" << std::endl;
    std::cout << "  curl -k https://localhost:8443/hello" << std::endl;
    std::cout << "  curl -k https://localhost:8443/secure" << std::endl;
    
    // Example of making HTTPS request to a real server (like google.com)
    std::cout << "\n[Client] Testing HTTPS with a real server (https://www.google.com)..." << std::endl;
    auto response = Get("https://www.google.com");
    if (response.Ok()) {
        std::cout << "[Client] Status: " << response.value.status_code << " " 
                  << response.value.status << std::endl;
        std::cout << "[Client] Successfully connected via HTTPS!" << std::endl;
        std::cout << "[Client] Response size: " << response.value.body.size() << " bytes" << std::endl;
    } else {
        std::cerr << "[Client] Failed: " << response.err->error() << std::endl;
    }
}

int main() {
    std::cout << "=== gocxx HTTPS Server/Client Example ===" << std::endl;
    std::cout << "This example demonstrates SSL/TLS support using OpenSSL" << std::endl;
    std::cout << std::endl;
    
    // Run server in a separate thread
    std::thread server_thread(runHTTPSServer);
    server_thread.detach();
    
    // Run client
    runHTTPSClient();
    
    std::cout << "\n=== HTTPS Example Information ===" << std::endl;
    std::cout << "Server is running on https://localhost:8443" << std::endl;
    std::cout << "Press Ctrl+C to exit." << std::endl;
    
    // Keep main thread alive
    std::this_thread::sleep_for(5s);
    
    return 0;
}
