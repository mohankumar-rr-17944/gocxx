#include <gocxx/net/http.h>
#include <gocxx/net/tls.h>
#include <sstream>
#include <algorithm>
#include <thread>
#include <cctype>

namespace gocxx::net::http {

// Helper functions
static std::string trim(const std::string& str) {
    size_t start = 0;
    size_t end = str.length();
    
    while (start < end && std::isspace(str[start])) {
        ++start;
    }
    
    while (end > start && std::isspace(str[end - 1])) {
        --end;
    }
    
    return str.substr(start, end - start);
}

static std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

// Request implementation
std::string Request::Header(const std::string& key) const {
    auto it = header.find(toLower(key));
    if (it != header.end()) {
        return it->second;
    }
    return "";
}

// Response implementation
std::string Response::Header(const std::string& key) const {
    auto it = header.find(toLower(key));
    if (it != header.end()) {
        return it->second;
    }
    return "";
}

// ResponseWriter implementation (internal)
class ResponseWriterImpl : public ResponseWriter {
public:
    ResponseWriterImpl(std::shared_ptr<TCPConn> conn)
        : conn_(conn), status_code_(200), headers_written_(false) {}
    
    std::map<std::string, std::string>& Header() override {
        return headers_;
    }
    
    gocxx::base::Result<std::size_t> Write(const std::string& data) override {
        if (!headers_written_) {
            WriteHeader(status_code_);
        }
        
        return conn_->Write(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    }
    
    void WriteHeader(int statusCode) override {
        if (headers_written_) {
            return;
        }
        
        status_code_ = statusCode;
        headers_written_ = true;
        
        // Build response
        std::ostringstream response;
        response << "HTTP/1.1 " << statusCode << " ";
        
        // Status text
        switch (statusCode) {
            case 200: response << "OK"; break;
            case 201: response << "Created"; break;
            case 400: response << "Bad Request"; break;
            case 404: response << "Not Found"; break;
            case 500: response << "Internal Server Error"; break;
            default: response << "Unknown"; break;
        }
        response << "\r\n";
        
        // Headers
        for (const auto& [key, value] : headers_) {
            response << key << ": " << value << "\r\n";
        }
        
        response << "\r\n";
        
        std::string response_str = response.str();
        conn_->Write(reinterpret_cast<const uint8_t*>(response_str.data()), 
                    response_str.size());
    }

private:
    std::shared_ptr<TCPConn> conn_;
    int status_code_;
    bool headers_written_;
    std::map<std::string, std::string> headers_;
};

// ServeMux implementation
void ServeMux::HandleFunc(const std::string& pattern, HandlerFunc handler) {
    handlers_[pattern] = handler;
}

void ServeMux::ServeHTTP(ResponseWriter& w, const Request& req) {
    // Simple exact match for now
    auto it = handlers_.find(req.url);
    if (it != handlers_.end()) {
        it->second(w, req);
        return;
    }
    
    // Try prefix match
    std::string longest_match;
    HandlerFunc matched_handler;
    
    for (const auto& [pattern, handler] : handlers_) {
        if (req.url.find(pattern) == 0 && pattern.length() > longest_match.length()) {
            longest_match = pattern;
            matched_handler = handler;
        }
    }
    
    if (!longest_match.empty()) {
        matched_handler(w, req);
        return;
    }
    
    // 404 Not Found
    w.WriteHeader(StatusNotFound);
    w.Write("404 page not found\n");
}

// Server implementation
gocxx::base::Result<void> Server::ListenAndServe() {
    auto listener_result = ListenTCP("tcp", addr);
    if (listener_result.Failed()) {
        return {listener_result.err};
    }
    
    auto listener = listener_result.value;
    
    while (true) {
        auto conn_result = listener->Accept();
        if (conn_result.Failed()) {
            // Log error but continue
            continue;
        }
        
        auto conn = std::dynamic_pointer_cast<TCPConn>(conn_result.value);
        if (!conn) {
            continue;
        }
        
        // Handle connection in a new thread
        std::thread([this, conn]() {
            handleConnection(conn);
        }).detach();
    }
    
    return {};
}

void Server::handleConnection(std::shared_ptr<TCPConn> conn) {
    // Read request
    const size_t BUFFER_SIZE = 4096;
    uint8_t buffer[BUFFER_SIZE];
    std::string raw_request;
    
    while (true) {
        auto read_result = conn->Read(buffer, BUFFER_SIZE);
        if (read_result.Failed() || read_result.value == 0) {
            break;
        }
        
        raw_request.append(reinterpret_cast<char*>(buffer), read_result.value);
        
        // Check if we have a complete request (simple check for \r\n\r\n)
        if (raw_request.find("\r\n\r\n") != std::string::npos) {
            break;
        }
        
        // Limit request size to prevent DoS
        if (raw_request.size() > 1024 * 1024) {  // 1MB limit
            break;
        }
    }
    
    // Parse request
    auto req_result = parseRequest(raw_request);
    if (req_result.Failed()) {
        conn->close();
        return;
    }
    
    auto request = req_result.value;
    request.remote_addr = conn->RemoteAddr()->String();
    
    // Handle request
    ResponseWriterImpl writer(conn);
    if (handler) {
        handler->ServeHTTP(writer, request);
    }
    
    conn->close();
}

gocxx::base::Result<Request> Server::parseRequest(const std::string& raw_request) {
    Request req;
    
    std::istringstream stream(raw_request);
    std::string line;
    
    // Parse request line
    if (!std::getline(stream, line)) {
        return {req, gocxx::errors::New("invalid request: no request line")};
    }
    
    // Remove \r if present
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    std::istringstream request_line(line);
    request_line >> req.method >> req.url >> req.proto;
    
    if (req.method.empty() || req.url.empty()) {
        return {req, gocxx::errors::New("invalid request line")};
    }
    
    // Parse headers
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (line.empty()) {
            break;  // End of headers
        }
        
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = toLower(trim(line.substr(0, colon_pos)));
            std::string value = trim(line.substr(colon_pos + 1));
            req.header[key] = value;
        }
    }
    
    // Parse body (rest of the stream)
    std::ostringstream body_stream;
    body_stream << stream.rdbuf();
    req.body = body_stream.str();
    
    return {req, nullptr};
}

// Global default ServeMux
static std::shared_ptr<ServeMux> default_mux = std::make_shared<ServeMux>();

std::shared_ptr<ServeMux> DefaultServeMux() {
    return default_mux;
}

void HandleFunc(const std::string& pattern, HandlerFunc handler) {
    default_mux->HandleFunc(pattern, handler);
}

gocxx::base::Result<void> ListenAndServe(
    const std::string& addr,
    std::shared_ptr<ServeMux> handler) {
    
    Server server(addr, handler);
    return server.ListenAndServe();
}

// HTTP client functions
gocxx::base::Result<Response> Get(const std::string& url) {
    Response resp;
    
    // Parse URL (supports both http:// and https://)
    std::string address;
    std::string path = "/";
    bool is_https = false;
    int default_port = 80;
    
    // Simple URL parsing
    if (url.find("https://") == 0) {
        is_https = true;
        default_port = 443;
        std::string rest = url.substr(8);  // Remove "https://"
        size_t slash_pos = rest.find('/');
        
        if (slash_pos != std::string::npos) {
            address = rest.substr(0, slash_pos);
            path = rest.substr(slash_pos);
        } else {
            address = rest;
        }
    } else if (url.find("http://") == 0) {
        std::string rest = url.substr(7);  // Remove "http://"
        size_t slash_pos = rest.find('/');
        
        if (slash_pos != std::string::npos) {
            address = rest.substr(0, slash_pos);
            path = rest.substr(slash_pos);
        } else {
            address = rest;
        }
    } else {
        return {resp, gocxx::errors::New("invalid URL: must start with http:// or https://")};
    }
    
    // Add default port if not specified
    if (address.find(':') == std::string::npos) {
        address += ":" + std::to_string(default_port);
    }
    
    // Connect (either TCP or TLS)
    std::shared_ptr<TCPConn> conn;
    
    if (is_https) {
        TLSConfig config;
        config.insecure_skip_verify = false;  // Verify certificates by default
        
        auto tls_result = DialTLS("tcp", address, &config);
        if (tls_result.Failed()) {
            return {resp, tls_result.err};
        }
        conn = tls_result.value;
    } else {
        auto tcp_result = DialTCP("tcp", address);
        if (tcp_result.Failed()) {
            return {resp, tcp_result.err};
        }
        conn = tcp_result.value;
    }
    
    // Send request
    std::ostringstream request;
    request << "GET " << path << " HTTP/1.1\r\n";
    request << "Host: " << address << "\r\n";
    request << "Connection: close\r\n";
    request << "\r\n";
    
    std::string request_str = request.str();
    auto write_result = conn->Write(
        reinterpret_cast<const uint8_t*>(request_str.data()),
        request_str.size());
    
    if (write_result.Failed()) {
        return {resp, write_result.err};
    }
    
    // Read response
    const size_t BUFFER_SIZE = 4096;
    uint8_t buffer[BUFFER_SIZE];
    std::string raw_response;
    
    while (true) {
        auto read_result = conn->Read(buffer, BUFFER_SIZE);
        if (read_result.Failed() || read_result.value == 0) {
            break;
        }
        
        raw_response.append(reinterpret_cast<char*>(buffer), read_result.value);
    }
    
    conn->close();
    
    // Parse response
    std::istringstream stream(raw_response);
    std::string line;
    
    // Parse status line
    if (!std::getline(stream, line)) {
        return {resp, gocxx::errors::New("invalid response: no status line")};
    }
    
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    std::istringstream status_line(line);
    status_line >> resp.proto >> resp.status_code;
    std::getline(status_line, resp.status);
    resp.status = trim(resp.status);
    
    // Parse headers
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (line.empty()) {
            break;  // End of headers
        }
        
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = toLower(trim(line.substr(0, colon_pos)));
            std::string value = trim(line.substr(colon_pos + 1));
            resp.header[key] = value;
        }
    }
    
    // Parse body
    std::ostringstream body_stream;
    body_stream << stream.rdbuf();
    resp.body = body_stream.str();
    
    return {resp, nullptr};
}

gocxx::base::Result<Response> Post(
    const std::string& url,
    const std::string& content_type,
    const std::string& body) {
    
    Response resp;
    
    // Parse URL (supports both http:// and https://)
    std::string address;
    std::string path = "/";
    bool is_https = false;
    int default_port = 80;
    
    if (url.find("https://") == 0) {
        is_https = true;
        default_port = 443;
        std::string rest = url.substr(8);  // Remove "https://"
        size_t slash_pos = rest.find('/');
        
        if (slash_pos != std::string::npos) {
            address = rest.substr(0, slash_pos);
            path = rest.substr(slash_pos);
        } else {
            address = rest;
        }
    } else if (url.find("http://") == 0) {
        std::string rest = url.substr(7);
        size_t slash_pos = rest.find('/');
        
        if (slash_pos != std::string::npos) {
            address = rest.substr(0, slash_pos);
            path = rest.substr(slash_pos);
        } else {
            address = rest;
        }
    } else {
        return {resp, gocxx::errors::New("invalid URL: must start with http:// or https://")};
    }
    
    if (address.find(':') == std::string::npos) {
        address += ":" + std::to_string(default_port);
    }
    
    // Connect (either TCP or TLS)
    std::shared_ptr<TCPConn> conn;
    
    if (is_https) {
        TLSConfig config;
        config.insecure_skip_verify = false;
        
        auto tls_result = DialTLS("tcp", address, &config);
        if (tls_result.Failed()) {
            return {resp, tls_result.err};
        }
        conn = tls_result.value;
    } else {
        auto tcp_result = DialTCP("tcp", address);
        if (tcp_result.Failed()) {
            return {resp, tcp_result.err};
        }
        conn = tcp_result.value;
    }
    
    // Send request
    std::ostringstream request;
    request << "POST " << path << " HTTP/1.1\r\n";
    request << "Host: " << address << "\r\n";
    request << "Content-Type: " << content_type << "\r\n";
    request << "Content-Length: " << body.length() << "\r\n";
    request << "Connection: close\r\n";
    request << "\r\n";
    request << body;
    
    std::string request_str = request.str();
    auto write_result = conn->Write(
        reinterpret_cast<const uint8_t*>(request_str.data()),
        request_str.size());
    
    if (write_result.Failed()) {
        return {resp, write_result.err};
    }
    
    // Read response (same as GET)
    const size_t BUFFER_SIZE = 4096;
    uint8_t buffer[BUFFER_SIZE];
    std::string raw_response;
    
    while (true) {
        auto read_result = conn->Read(buffer, BUFFER_SIZE);
        if (read_result.Failed() || read_result.value == 0) {
            break;
        }
        
        raw_response.append(reinterpret_cast<char*>(buffer), read_result.value);
    }
    
    conn->close();
    
    // Parse response (same logic as GET)
    std::istringstream stream(raw_response);
    std::string line;
    
    if (!std::getline(stream, line)) {
        return {resp, gocxx::errors::New("invalid response: no status line")};
    }
    
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    std::istringstream status_line(line);
    status_line >> resp.proto >> resp.status_code;
    std::getline(status_line, resp.status);
    resp.status = trim(resp.status);
    
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (line.empty()) {
            break;
        }
        
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = toLower(trim(line.substr(0, colon_pos)));
            std::string value = trim(line.substr(colon_pos + 1));
            resp.header[key] = value;
        }
    }
    
    std::ostringstream body_stream;
    body_stream << stream.rdbuf();
    resp.body = body_stream.str();
    
    return {resp, nullptr};
}

// HTTPS Server function
gocxx::base::Result<void> ListenAndServeTLS(
    const std::string& addr,
    const std::string& cert_file,
    const std::string& key_file,
    std::shared_ptr<ServeMux> handler) {
    
    // Create TLS configuration
    TLSConfig config;
    config.cert_file = cert_file;
    config.key_file = key_file;
    
    // Create TLS listener
    auto listener_result = ListenTLS("tcp", addr, config);
    if (listener_result.Failed()) {
        return {listener_result.err};
    }
    
    auto listener = listener_result.value;
    
    // Accept and handle connections (same as HTTP server)
    while (true) {
        auto conn_result = listener->Accept();
        if (conn_result.Failed()) {
            // Log error but continue
            continue;
        }
        
        auto conn = std::dynamic_pointer_cast<TCPConn>(conn_result.value);
        if (!conn) {
            continue;
        }
        
        // Handle connection in a new thread
        std::thread([handler, conn]() {
            // Read request
            const size_t BUFFER_SIZE = 4096;
            uint8_t buffer[BUFFER_SIZE];
            std::string raw_request;
            
            while (true) {
                auto read_result = conn->Read(buffer, BUFFER_SIZE);
                if (read_result.Failed() || read_result.value == 0) {
                    break;
                }
                
                raw_request.append(reinterpret_cast<char*>(buffer), read_result.value);
                
                // Check if we have a complete request
                if (raw_request.find("\r\n\r\n") != std::string::npos) {
                    break;
                }
                
                // Limit request size to prevent DoS
                if (raw_request.size() > 1024 * 1024) {
                    break;
                }
            }
            
            // Parse request (reuse Server's parseRequest logic)
            // For simplicity, we'll inline a basic parser here
            Request req;
            std::istringstream stream(raw_request);
            std::string line;
            
            // Parse request line
            if (std::getline(stream, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                
                std::istringstream request_line(line);
                request_line >> req.method >> req.url >> req.proto;
            }
            
            // Parse headers
            while (std::getline(stream, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                
                if (line.empty()) {
                    break;
                }
                
                size_t colon_pos = line.find(':');
                if (colon_pos != std::string::npos) {
                    std::string key = toLower(trim(line.substr(0, colon_pos)));
                    std::string value = trim(line.substr(colon_pos + 1));
                    req.header[key] = value;
                }
            }
            
            // Parse body
            std::ostringstream body_stream;
            body_stream << stream.rdbuf();
            req.body = body_stream.str();
            
            req.remote_addr = conn->RemoteAddr()->String();
            
            // Handle request
            ResponseWriterImpl writer(conn);
            if (handler) {
                handler->ServeHTTP(writer, req);
            }
            
            conn->close();
        }).detach();
    }
    
    return {};
}

} // namespace gocxx::net::http
