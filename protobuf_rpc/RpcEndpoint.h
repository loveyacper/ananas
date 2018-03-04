#ifndef BERT_RPCENDPOINT_H
#define BERT_RPCENDPOINT_H

#include <string>
#include "net/Socket.h"

namespace ananas
{

namespace rpc
{

struct Endpoint
{
public:
    Endpoint()
    {
    }

    Endpoint(const std::string& url)
    {
        FromString(url);
    }

    // format:  tcp://127.0.0.1:1234
    bool FromString(const std::string& url);
    std::string ToString() const;

    bool IsValid() const { return addr.IsValid(); }

public:
    enum {
        TCP,
        UDP,
        SSL,
    } proto;

    ananas::SocketAddr addr;
};



inline
bool Endpoint::FromString(const std::string& url)
{
    this->addr.Clear();

    // len(tcp://1.1.1.1:1) = 15
    if (url.size() < 15)
        return false;

    std::string::size_type sep = url.rfind('/');
    if (sep == std::string::npos)
        return false;

    if (strncmp(url.data(), "tcp", 3) == 0)
        proto = TCP;
    else if (strncmp(url.data(), "udp", 3) == 0)
        proto = UDP;
    else if (strncmp(url.data(), "ssl", 3) == 0)
        proto = SSL;
    else
        return false;

    this->addr.Init(url.substr(sep + 1));
    return true;
}


inline
std::string Endpoint::ToString() const
{
    if (!IsValid())
        return "Invalid Endpoint";

    std::string rep;
    if (proto == TCP)
        rep = "tcp://";
    else if (proto == UDP)
        rep = "udp://";
    else
        rep = "ssl://";

    rep += addr.ToString();
    return rep;
}

} // end namespace rpc

} // end namespace ananas

namespace std
{
    template<>
    struct hash<ananas::rpc::Endpoint>
    {
        typedef ananas::rpc::Endpoint argument_type;
        typedef std::size_t result_type;
        result_type operator()(const argument_type& s) const noexcept
        {
            result_type h1 = std::hash<short>{}(s.proto);
            result_type h2 = std::hash<ananas::SocketAddr>{}(s.addr);
            return h1 ^ (h2 << 1);
        }
    };
}

#endif

