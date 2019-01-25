#ifndef BERT_RPCENDPOINT_H
#define BERT_RPCENDPOINT_H

#include <string>
#include "ananas/net/Socket.h"
#include "ananas_rpc.pb.h"

///@file RpcEndpoint.h
namespace ananas {

namespace rpc {

///@brief Convert endpoint to socket address
inline
SocketAddr EndpointToSocketAddr(const Endpoint& ep) {
    return SocketAddr(ep.ip().data(), ep.port());
}

///@brief Construct endpoint from string format:
/// tcp://127.0.0.1:8000
inline
Endpoint EndpointFromString(const std::string& url) {
    // len(tcp://1.1.1.1:1) = 15
    Endpoint ep;
    if (url.size() < 15)
        return ep;

    std::string::size_type sep = url.rfind('/');
    if (sep == std::string::npos)
        return ep;

    if (strncmp(url.data(), "tcp", 3) == 0)
        ep.set_proto(TCP);
    else if (strncmp(url.data(), "udp", 3) == 0)
        ep.set_proto(UDP);
    else if (strncmp(url.data(), "ssl", 3) == 0)
        ep.set_proto(SSL);
    else
        return ep;

    std::string ipport = url.substr(sep + 1);
    std::string::size_type p = ipport.find_first_of(':');
    if (p == std::string::npos)
        return ep;

    std::string host(ipport.substr(0, p));
    ep.set_ip(ConvertIp(host.data()));
    ep.set_port(std::stoi(ipport.substr(p + 1)));
    return ep;
}

///@brief Convert endpoint to string format:
/// tcp://127.0.0.1:8000
inline
std::string EndpointToString(const Endpoint& ep) {
    std::string rep;
    if (ep.proto() == TCP)
        rep = "tcp://";
    else if (ep.proto() == UDP)
        rep = "udp://";
    else
        rep = "ssl://";

    rep += ep.ip() + ":" + std::to_string(ep.port());
    return rep;
}

inline
bool operator==(const Endpoint& a, const Endpoint& b) {
    return a.proto() == b.proto() &&
           a.ip() == b.ip() &&
           a.port() == b.port();
}

///@brief Check it whether valid endpoint
inline
bool IsValidEndpoint(const Endpoint& ep) {
    return !ep.ip().empty() && ep.port() > 0;
}

} // end namespace rpc

} // end namespace ananas

namespace std {
template<>
struct hash<ananas::rpc::Endpoint> {
    typedef ananas::rpc::Endpoint argument_type;
    typedef std::size_t result_type;
    result_type operator()(const argument_type& s) const noexcept {
        result_type h1 = std::hash<int> {}(s.proto());
        result_type h2 = std::hash<int> {}(s.port());
        result_type h3 = std::hash<std::string> {}(s.ip());
        result_type tmp = h1 ^ (h2 << 1);
        return h3 ^ (tmp << 1);
    }
};
}

#endif

