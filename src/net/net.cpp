#include <gocxx/net/net.h>

namespace gocxx::net {

// Common network errors
std::shared_ptr<gocxx::errors::Error> ErrClosed = 
    gocxx::errors::New("connection closed");
std::shared_ptr<gocxx::errors::Error> ErrTimeout = 
    gocxx::errors::New("i/o timeout");
std::shared_ptr<gocxx::errors::Error> ErrInvalidAddr = 
    gocxx::errors::New("invalid network address");

} // namespace gocxx::net
