/**
 * @file http_example.cpp
 * @brief Example demonstrating gocxx HTTP server and client
 */

#include <gocxx/gocxx.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace gocxx::net::http;
using namespace std::chrono_literals;

// HTTP Server
void runHTTPServer() {
    std::cout << "[Server] Starting HTTP server on :8888..." << std::endl;
    
    auto mux = std::make_shared<ServeMux>();
    
    // Handler for root path
    mux->HandleFunc("/", [](ResponseWriter& w, const Request& req) {
        std::cout << "[Server] Handling request: " << req.method << " " << req.url << std::endl;
        w.Header()["content-type"] = "text/html";
        w.Write("<html><body><h1>Welcome to gocxx HTTP Server!</h1>");
        w.Write("<p>Available endpoints:</p>");
        w.Write("<ul>");
        w.Write("<li><a href='/hello'>/hello</a> - Simple greeting</li>");
        w.Write("<li><a href='/echo'>/echo</a> - Echo endpoint</li>");
        w.Write("<li><a href='/info'>/info</a> - Request information</li>");
        w.Write("</ul></body></html>");
    });
    
    // Handler for /hello
    mux->HandleFunc("/hello", [](ResponseWriter& w, const Request& req) {
        std::cout << "[Server] Handling /hello request" << std::endl;
        w.Header()["content-type"] = "text/plain";
        w.Write("Hello from gocxx HTTP server!\n");
    });
    
    // Handler for /echo
    mux->HandleFunc("/echo", [](ResponseWriter& w, const Request& req) {
        std::cout << "[Server] Handling /echo request" << std::endl;
        w.Header()["content-type"] = "text/plain";
        
        if (req.method == "POST") {
            w.Write("Echo: " + req.body);
        } else {
            w.Write("Send a POST request with a body to echo it back.\n");
        }
    });
    
    // Handler for /info
    mux->HandleFunc("/info", [](ResponseWriter& w, const Request& req) {
        std::cout << "[Server] Handling /info request" << std::endl;
        w.Header()["content-type"] = "text/plain";
        
        w.Write("Request Information:\n");
        w.Write("Method: " + req.method + "\n");
        w.Write("URL: " + req.url + "\n");
        w.Write("Protocol: " + req.proto + "\n");
        w.Write("Remote Address: " + req.remote_addr + "\n");
        w.Write("\nHeaders:\n");
        for (const auto& [key, value] : req.header) {
            w.Write("  " + key + ": " + value + "\n");
        }
    });
    
    // Start server (this blocks)
    auto result = ListenAndServe(":8888", mux);
    if (result.Failed()) {
        std::cerr << "[Server] Server error: " << result.err->error() << std::endl;
    }
}

// HTTP Client
void runHTTPClient() {
    // Give server time to start
    std::this_thread::sleep_for(1s);
    
    std::cout << "[Client] Making HTTP requests..." << std::endl;
    
    // Test GET request to /hello
    std::cout << "\n[Client] GET http://localhost:8888/hello" << std::endl;
    auto response1 = Get("http://localhost:8888/hello");
    if (response1.Ok()) {
        std::cout << "[Client] Status: " << response1.value.status_code << " " 
                  << response1.value.status << std::endl;
        std::cout << "[Client] Body: " << response1.value.body << std::endl;
    } else {
        std::cerr << "[Client] Failed: " << response1.err->error() << std::endl;
    }
    
    std::this_thread::sleep_for(200ms);
    
    // Test POST request to /echo
    std::cout << "\n[Client] POST http://localhost:8888/echo" << std::endl;
    auto response2 = Post("http://localhost:8888/echo", 
                         "text/plain", 
                         "This is a test message!");
    if (response2.Ok()) {
        std::cout << "[Client] Status: " << response2.value.status_code << " " 
                  << response2.value.status << std::endl;
        std::cout << "[Client] Body: " << response2.value.body << std::endl;
    } else {
        std::cerr << "[Client] Failed: " << response2.err->error() << std::endl;
    }
    
    std::this_thread::sleep_for(200ms);
    
    // Test GET request to /info
    std::cout << "\n[Client] GET http://localhost:8888/info" << std::endl;
    auto response3 = Get("http://localhost:8888/info");
    if (response3.Ok()) {
        std::cout << "[Client] Status: " << response3.value.status_code << " " 
                  << response3.value.status << std::endl;
        std::cout << "[Client] Body:\n" << response3.value.body << std::endl;
    } else {
        std::cerr << "[Client] Failed: " << response3.err->error() << std::endl;
    }
}

int main() {
    std::cout << "=== gocxx HTTP Server/Client Example ===" << std::endl;
    
    // Run server in a separate thread
    // Note: The server will run indefinitely, so we'll let it run in the background
    std::thread server_thread(runHTTPServer);
    server_thread.detach(); // Detach so main can exit
    
    // Run client in main thread
    runHTTPClient();
    
    std::cout << "\n=== HTTP Client Example completed ===" << std::endl;
    std::cout << "Note: Server is still running. Press Ctrl+C to exit." << std::endl;
    
    // Keep main thread alive for a bit to show server is running
    std::this_thread::sleep_for(2s);
    
    return 0;
}
