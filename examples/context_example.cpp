/**
 * @file context_example.cpp
 * @brief Example demonstrating gocxx context usage for cancellation and timeouts
 */

#include <gocxx/gocxx.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace gocxx::context;
using namespace gocxx::time;

// Simulate a long-running operation that respects context cancellation
bool longRunningOperation(std::shared_ptr<Context> ctx, const std::string& name) {
    std::cout << "[" << name << "] Starting long operation..." << std::endl;
    
    for (int i = 0; i < 10; ++i) {
        // Check for cancellation - Done() returns a channel we can check
        auto done = ctx->Done();
        auto canceled = done.tryRecv();
        if (canceled.has_value()) {
            std::cout << "[" << name << "] Operation canceled!" << std::endl;
            return false;
        }
        
        std::cout << "[" << name << "] Working... step " << (i + 1) << "/10" << std::endl;
        
        // Simulate work
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    
    std::cout << "[" << name << "] Operation completed successfully!" << std::endl;
    return true;
}

int main() {
    std::cout << "=== gocxx Context Examples ===" << std::endl;
    
    // Example 1: Basic timeout context
    std::cout << "\n--- Example 1: Timeout Context ---" << std::endl;
    {
        auto result = WithTimeout(Background(), Duration(1 * Duration::Second));
        if (result.Ok()) {
            auto [ctx, cancel] = result.value;
            
            // This should be canceled due to timeout
            longRunningOperation(ctx, "TimeoutExample");
        }
    }
    
    // Example 2: Manual cancellation
    std::cout << "\n--- Example 2: Manual Cancellation ---" << std::endl;
    {
        auto result = WithCancel(Background());
        if (result.Ok()) {
            auto [ctx, cancel] = result.value;
            
            // Start operation in a separate thread
            std::thread worker([ctx]() {
                longRunningOperation(ctx, "ManualExample");
            });
            
            // Cancel after 1 second
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "Manually canceling operation..." << std::endl;
            cancel();
            
            worker.join();
        }
    }
    
    // Example 3: Context with values
    std::cout << "\n--- Example 3: Context with Values ---" << std::endl;
    {
        // Create context with request metadata
        auto ctx_result = WithValue(Background(), std::string("request_id"), std::string("REQ-12345"));
        if (ctx_result.Ok()) {
            auto ctx = ctx_result.value;
            
            // Extract value
            auto request_id_result = ctx->Value(std::string("request_id"));
            if (request_id_result.Ok()) {
                try {
                    std::string id = std::any_cast<std::string>(request_id_result.value);
                    std::cout << "Processing request: " << id << std::endl;
                } catch (...) {
                    std::cout << "Failed to cast request_id" << std::endl;
                }
            }
            
            // Add timeout
            auto timeout_result = WithTimeout(ctx, Duration(800 * Duration::Millisecond));
            if (timeout_result.Ok()) {
                auto [timeout_ctx, cancel] = timeout_result.value;
                longRunningOperation(timeout_ctx, "ValueExample");
            }
        }
    }
    
    // Example 4: Background context
    std::cout << "\n--- Example 4: Background Context ---" << std::endl;
    {
        auto ctx = Background();
        std::cout << "Background context never cancels" << std::endl;
    }
    
    std::cout << "\n=== All examples completed ===" << std::endl;
    return 0;
}
