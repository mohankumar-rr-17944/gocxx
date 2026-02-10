#pragma once

/**
 * @file http.h
 * @brief HTTP client and server support (Go-style)
 * 
 * Provides basic HTTP functionality:
 * - http::Get, http::Post: Client functions
 * - http::Server, http::ServeMux: Server components
 * - http::Request, http::Response: HTTP messages
 * - http::ListenAndServe: Simplified server startup
 */

#include <gocxx/net/net.h>
#include <gocxx/net/tcp.h>
#include <string>
#include <map>
#include <memory>
#include <functional>
#include <gocxx/base/chan.h>

namespace gocxx::net::http {

// Forward declarations
class Request;
class Response;
class ResponseWriter;
class ServeMux;

/**
 * @brief HTTP handler function type
 * 
 * Similar to Go's http.HandlerFunc
 */
using HandlerFunc = std::function<void(ResponseWriter&, const Request&)>;

/**
 * @brief HTTP request
 */
class Request {
public:
    std::string method;          ///< HTTP method (GET, POST, etc.)
    std::string url;             ///< Request URL/path
    std::string proto;           ///< Protocol version (HTTP/1.0, HTTP/1.1)
    std::map<std::string, std::string> header;  ///< HTTP headers
    std::string body;            ///< Request body
    std::string remote_addr;     ///< Remote address
    
    Request() = default;
    
    /**
     * @brief Gets a header value
     */
    std::string Header(const std::string& key) const;
};

/**
 * @brief HTTP response writer interface
 */
class ResponseWriter {
public:
    virtual ~ResponseWriter() = default;
    
    /**
     * @brief Gets the header map for modification
     */
    virtual std::map<std::string, std::string>& Header() = 0;
    
    /**
     * @brief Writes data to the response body
     */
    virtual gocxx::base::Result<std::size_t> Write(const std::string& data) = 0;
    
    /**
     * @brief Writes the HTTP status code
     */
    virtual void WriteHeader(int statusCode) = 0;
};

/**
 * @brief HTTP response (for client)
 */
class Response {
public:
    std::string proto;           ///< Protocol version
    int status_code;             ///< HTTP status code
    std::string status;          ///< Status text
    std::map<std::string, std::string> header;  ///< HTTP headers
    std::string body;            ///< Response body
    
    Response() : status_code(0) {}
    
    /**
     * @brief Gets a header value
     */
    std::string Header(const std::string& key) const;
};

/**
 * @brief HTTP server multiplexer (router)
 */
class ServeMux {
public:
    ServeMux() = default;
    
    /**
     * @brief Registers a handler for a pattern
     * 
     * @param pattern URL pattern to match
     * @param handler Handler function
     */
    void HandleFunc(const std::string& pattern, HandlerFunc handler);
    
    /**
     * @brief Serves an HTTP request
     */
    void ServeHTTP(ResponseWriter& w, const Request& req);

private:
    std::map<std::string, HandlerFunc> handlers_;
};

/**
 * @brief HTTP server
 */
class Server {
public:
    std::string addr;                    ///< Server address
    std::shared_ptr<ServeMux> handler;   ///< Request multiplexer
    
    Server(const std::string& addr, std::shared_ptr<ServeMux> mux)
        : addr(addr), handler(mux) {}
    
    /**
     * @brief Starts the server and listens for requests
     */
    gocxx::base::Result<void> ListenAndServe();

private:
    void handleConnection(std::shared_ptr<TCPConn> conn);
    gocxx::base::Result<Request> parseRequest(const std::string& raw_request);
};

/**
 * @brief Convenience function to start an HTTP server
 * 
 * Similar to Go's http.ListenAndServe(addr, handler)
 * 
 * @param addr Address to listen on (e.g., ":8080")
 * @param handler Request multiplexer
 * @return Result containing void or an error
 */
gocxx::base::Result<void> ListenAndServe(
    const std::string& addr,
    std::shared_ptr<ServeMux> handler);

/**
 * @brief Registers a handler function for a pattern on the default mux
 * 
 * Similar to Go's http.HandleFunc(pattern, handler)
 * 
 * @param pattern URL pattern to match
 * @param handler Handler function
 */
void HandleFunc(const std::string& pattern, HandlerFunc handler);

/**
 * @brief Gets the default ServeMux instance
 */
std::shared_ptr<ServeMux> DefaultServeMux();

/**
 * @brief Performs an HTTP GET request
 * 
 * Similar to Go's http.Get(url)
 * 
 * @param url URL to fetch
 * @return Result containing Response or an error
 */
gocxx::base::Result<Response> Get(const std::string& url);

/**
 * @brief Performs an HTTP POST request
 * 
 * Similar to Go's http.Post(url, contentType, body)
 * 
 * @param url URL to post to
 * @param content_type Content-Type header value
 * @param body Request body
 * @return Result containing Response or an error
 */
gocxx::base::Result<Response> Post(
    const std::string& url,
    const std::string& content_type,
    const std::string& body);

// HTTP status codes (common ones)
const int StatusOK = 200;
const int StatusCreated = 201;
const int StatusBadRequest = 400;
const int StatusNotFound = 404;
const int StatusInternalServerError = 500;

} // namespace gocxx::net::http
