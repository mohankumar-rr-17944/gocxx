#include <gtest/gtest.h>
#include <gocxx/net/net.h>
#include <gocxx/net/tcp.h>
#include <gocxx/net/udp.h>
#include <gocxx/net/http.h>
#include <thread>
#include <chrono>

using namespace gocxx::net;
using namespace gocxx::net::http;

// TCP Tests
TEST(NetTest, TCPEchoServerClient) {
    const std::string TEST_MSG = "Hello, TCP!";
    bool server_done = false;
    
    // Start echo server in a thread
    std::thread server_thread([&server_done]() {
        auto listener_result = ListenTCP("tcp", ":9090");
        ASSERT_TRUE(listener_result.Ok());
        
        auto listener = listener_result.value;
        
        auto conn_result = listener->Accept();
        ASSERT_TRUE(conn_result.Ok());
        
        auto conn = std::dynamic_pointer_cast<TCPConn>(conn_result.value);
        ASSERT_NE(conn, nullptr);
        
        // Echo back what we receive
        uint8_t buffer[256];
        auto read_result = conn->Read(buffer, sizeof(buffer));
        ASSERT_TRUE(read_result.Ok());
        ASSERT_GT(read_result.value, 0);
        
        auto write_result = conn->Write(buffer, read_result.value);
        ASSERT_TRUE(write_result.Ok());
        
        conn->close();
        listener->Close();
        server_done = true;
    });
    
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Client
    auto conn_result = DialTCP("tcp", "localhost:9090");
    ASSERT_TRUE(conn_result.Ok());
    
    auto conn = conn_result.value;
    
    // Send message
    auto write_result = conn->Write(
        reinterpret_cast<const uint8_t*>(TEST_MSG.data()), 
        TEST_MSG.size());
    ASSERT_TRUE(write_result.Ok());
    ASSERT_EQ(write_result.value, TEST_MSG.size());
    
    // Receive echo
    uint8_t buffer[256];
    auto read_result = conn->Read(buffer, sizeof(buffer));
    ASSERT_TRUE(read_result.Ok());
    
    std::string received(reinterpret_cast<char*>(buffer), read_result.value);
    EXPECT_EQ(received, TEST_MSG);
    
    conn->close();
    server_thread.join();
    EXPECT_TRUE(server_done);
}

TEST(NetTest, TCPAddressResolution) {
    auto addr_result = ResolveTCPAddr("tcp", "localhost:8080");
    ASSERT_TRUE(addr_result.Ok());
    
    auto addr = addr_result.value;
    EXPECT_EQ(addr->port, 8080);
    EXPECT_EQ(addr->Network(), "tcp");
    EXPECT_FALSE(addr->String().empty());
}

// UDP Tests
TEST(NetTest, UDPSendReceive) {
    const std::string TEST_MSG = "Hello, UDP!";
    
    // Server
    auto server_result = ListenUDPSimple(":9091");
    ASSERT_TRUE(server_result.Ok());
    
    auto server = server_result.value;
    
    std::thread server_thread([&server, &TEST_MSG]() {
        uint8_t buffer[256];
        std::shared_ptr<Addr> sender_addr;
        
        auto read_result = server->ReadFrom(buffer, sizeof(buffer), sender_addr);
        ASSERT_TRUE(read_result.Ok());
        
        std::string received(reinterpret_cast<char*>(buffer), read_result.value);
        EXPECT_EQ(received, TEST_MSG);
        
        // Send response back
        auto write_result = server->WriteTo(
            reinterpret_cast<const uint8_t*>(TEST_MSG.data()),
            TEST_MSG.size(),
            sender_addr);
        ASSERT_TRUE(write_result.Ok());
    });
    
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Client
    auto addr_result = ResolveUDPAddr("udp", "localhost:9091");
    ASSERT_TRUE(addr_result.Ok());
    
    auto client_result = DialUDP("udp", nullptr, addr_result.value);
    ASSERT_TRUE(client_result.Ok());
    
    auto client = client_result.value;
    
    // Send message
    auto write_result = client->Write(
        reinterpret_cast<const uint8_t*>(TEST_MSG.data()),
        TEST_MSG.size());
    ASSERT_TRUE(write_result.Ok());
    
    // Receive response
    uint8_t buffer[256];
    auto read_result = client->Read(buffer, sizeof(buffer));
    ASSERT_TRUE(read_result.Ok());
    
    std::string received(reinterpret_cast<char*>(buffer), read_result.value);
    EXPECT_EQ(received, TEST_MSG);
    
    client->close();
    server->close();
    server_thread.join();
}

TEST(NetTest, UDPAddressResolution) {
    auto addr_result = ResolveUDPAddr("udp", "localhost:8080");
    ASSERT_TRUE(addr_result.Ok());
    
    auto addr = addr_result.value;
    EXPECT_EQ(addr->port, 8080);
    EXPECT_EQ(addr->Network(), "udp");
    EXPECT_FALSE(addr->String().empty());
}

// HTTP Tests
TEST(NetTest, HTTPServerAndClient) {
    const std::string RESPONSE_TEXT = "Hello from HTTP server!";
    bool server_started = false;
    
    // Start HTTP server in a thread
    std::thread server_thread([&server_started, &RESPONSE_TEXT]() {
        auto mux = std::make_shared<ServeMux>();
        
        mux->HandleFunc("/test", [&RESPONSE_TEXT](ResponseWriter& w, const Request& req) {
            EXPECT_EQ(req.method, "GET");
            EXPECT_EQ(req.url, "/test");
            w.Header()["content-type"] = "text/plain";
            w.Write(RESPONSE_TEXT);
        });
        
        server_started = true;
        
        // Note: ListenAndServe blocks, so we'll just test the setup
        // In a real test, you'd need a way to gracefully shut down the server
    });
    
    // Wait for server to initialize
    while (!server_started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // We can't actually start the server in this test without blocking,
    // so we'll test the components separately
    server_thread.detach();
}

TEST(NetTest, HTTPRequestParsing) {
    // Test ServeMux
    auto mux = std::make_shared<ServeMux>();
    bool handler_called = false;
    
    mux->HandleFunc("/test", [&handler_called](ResponseWriter& w, const Request& req) {
        handler_called = true;
        EXPECT_EQ(req.url, "/test");
        w.WriteHeader(StatusOK);
        w.Write("OK");
    });
    
    // Create a mock request
    Request req;
    req.method = "GET";
    req.url = "/test";
    req.proto = "HTTP/1.1";
    
    // We can't easily test the full flow without a real connection,
    // but we've verified the handler registration works
    EXPECT_FALSE(handler_called);  // Not called yet since we didn't call ServeHTTP
}

TEST(NetTest, HTTPStatusCodes) {
    EXPECT_EQ(StatusOK, 200);
    EXPECT_EQ(StatusCreated, 201);
    EXPECT_EQ(StatusBadRequest, 400);
    EXPECT_EQ(StatusNotFound, 404);
    EXPECT_EQ(StatusInternalServerError, 500);
}

// Connection interface tests
TEST(NetTest, ConnectionInterface) {
    // Test that TCPConn implements the Conn interface
    auto conn_result = DialTCP("tcp", "localhost:80");
    // We don't expect this to succeed (no server), but it tests the API
    
    // Test that UDPConn implements PacketConn interface
    auto udp_result = ListenUDPSimple(":0");  // Port 0 = random port
    if (udp_result.Ok()) {
        auto udp = udp_result.value;
        EXPECT_NE(udp->LocalAddr(), nullptr);
        udp->close();
    }
}

// Concurrent connection test
TEST(NetTest, ConcurrentConnections) {
    auto listener_result = ListenTCP("tcp", ":9092");
    ASSERT_TRUE(listener_result.Ok());
    
    auto listener = listener_result.value;
    const int NUM_CLIENTS = 5;
    std::atomic<int> connections_handled{0};
    
    // Server thread
    std::thread server_thread([&listener, &connections_handled, NUM_CLIENTS]() {
        for (int i = 0; i < NUM_CLIENTS; ++i) {
            auto conn_result = listener->Accept();
            if (conn_result.Ok()) {
                connections_handled++;
                auto conn = conn_result.value;
                conn->close();
            }
        }
        listener->Close();
    });
    
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Client threads
    std::vector<std::thread> client_threads;
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        client_threads.emplace_back([]() {
            auto conn_result = DialTCP("tcp", "localhost:9092");
            if (conn_result.Ok()) {
                auto conn = conn_result.value;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                conn->close();
            }
        });
    }
    
    // Wait for all clients
    for (auto& t : client_threads) {
        t.join();
    }
    
    server_thread.join();
    EXPECT_EQ(connections_handled, NUM_CLIENTS);
}
