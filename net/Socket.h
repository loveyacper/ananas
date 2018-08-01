
#ifndef BERT_SOCKET_H
#define BERT_SOCKET_H

#include <arpa/inet.h>
#include <sys/resource.h>
#include <string.h>
#include <string>
#include <memory>
#include <functional>

namespace ananas {

std::string ConvertIp(const char* ip);

struct SocketAddr {
    static const uint16_t kInvalidPort = -1;

    SocketAddr() {
        Clear();
    }

    SocketAddr(const sockaddr_in& addr) {
        Init(addr);
    }

    SocketAddr(uint32_t netip, uint16_t netport) {
        Init(netip, netport);
    }

    SocketAddr(const char* ip, uint16_t hostport) {
        Init(ip, hostport);
    }
    SocketAddr(const std::string& ip, uint16_t hostport) {
        Init(ip.data(), hostport);
    }

    SocketAddr(const std::string& ipport) {
        Init(ipport);
    }

    void Init(const sockaddr_in& addr) {
        memcpy(&addr_, &addr, sizeof(addr));
    }

    void Init(uint32_t netip, uint16_t netport) {
        addr_.sin_family = AF_INET;
        addr_.sin_addr.s_addr = netip;
        addr_.sin_port   = netport;
    }

    void Init(const char* ip, uint16_t hostport) {
        std::string sip = ConvertIp(ip);
        addr_.sin_family = AF_INET;
        addr_.sin_addr.s_addr = ::inet_addr(sip.data());
        addr_.sin_port = htons(hostport);
    }

    // ip port format:  127.0.0.1:6379
    void Init(const std::string& ipport) {
        std::string::size_type p = ipport.find_first_of(':');
        std::string ip = ipport.substr(0, p);
        std::string port = ipport.substr(p + 1);

        Init(ip.c_str(), static_cast<uint16_t>(std::stoi(port)));
    }

    const sockaddr_in& GetAddr() const {
        return addr_;
    }

    std::string GetIP() const {
        char tmp[32];
        const char* res = inet_ntop(AF_INET, &addr_.sin_addr,
                                    tmp, (socklen_t)(sizeof tmp));
        return std::string(res);
    }

    uint16_t GetPort() const {
        return ntohs(addr_.sin_port);
    }

    std::string ToString() const {
        char tmp[32];
        const char* res = inet_ntop(AF_INET, &addr_.sin_addr, tmp, (socklen_t)(sizeof tmp));

        return std::string(res) + ":" + std::to_string(ntohs(addr_.sin_port));
    }

    bool IsValid() const {
        return addr_.sin_family != 0;
    }

    void Clear() {
        memset(&addr_, 0, sizeof addr_);
    }

    inline friend bool operator== (const SocketAddr& a, const SocketAddr& b) {
        return a.addr_.sin_family      ==  b.addr_.sin_family &&
               a.addr_.sin_addr.s_addr ==  b.addr_.sin_addr.s_addr &&
               a.addr_.sin_port        ==  b.addr_.sin_port ;
    }

    inline friend bool operator!= (const SocketAddr& a, const SocketAddr& b) {
        return !(a == b);
    }

private:
    sockaddr_in  addr_;
};

extern const int kInvalid;

extern const int kTimeout;
extern const int kError;
extern const int kEof;

int CreateTCPSocket();
int CreateUDPSocket();
bool CreateSocketPair(int& readSock, int& writeSock);
void CloseSocket(int &sock);
void SetNonBlock(int sock, bool nonBlock = true);
void SetNodelay(int sock, bool enable = true);
void SetSndBuf(int sock, socklen_t size = 64 * 1024);
void SetRcvBuf(int sock, socklen_t size = 64 * 1024);
void SetReuseAddr(int sock);
bool GetLocalAddr(int sock, SocketAddr& );
bool GetPeerAddr(int sock, SocketAddr& );

in_addr_t GetLocalAddrInfo();

rlim_t GetMaxOpenFd();
bool SetMaxOpenFd(rlim_t maxfdPlus1);

} // end namespace ananas


namespace std {
template<>
struct hash<ananas::SocketAddr> {
    typedef ananas::SocketAddr argument_type;
    typedef std::size_t result_type;
    result_type operator()(const argument_type& s) const noexcept {
        result_type h1 = std::hash<short> {}(s.GetAddr().sin_family);
        result_type h2 = std::hash<unsigned short> {}(s.GetAddr().sin_port);
        result_type h3 = std::hash<unsigned int> {}(s.GetAddr().sin_addr.s_addr);
        result_type tmp = h1 ^ (h2 << 1);
        return h3 ^ (tmp << 1);
    }
};
}

#endif

