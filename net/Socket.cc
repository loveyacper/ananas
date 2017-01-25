#include <cassert>

#include <fcntl.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "Socket.h"

namespace ananas
{

const int kInvalid = -1;

const int kTimeout =  0;
const int kError = -1;
const int kEof = -2;

int CreateUDPSocket()
{
    return ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

int CreateTCPSocket()
{
    return ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

bool CreateSocketPair(int& readSock, int& writeSock)
{
    int s[2];
    int ret = socketpair(AF_LOCAL, SOCK_STREAM, IPPROTO_TCP, s);
    if (ret != 0)
        return false;

    readSock = s[0];
    writeSock = s[1];

    return true;
}

void CloseSocket(int& sock)
{
    if (sock != kInvalid)
    {
        ::shutdown(sock, SHUT_RDWR);
        ::close(sock);
        sock = kInvalid;
    }
}

void SetNonBlock(int sock, bool nonblock)
{
    int flag = ::fcntl(sock, F_GETFL, 0); 
    assert(flag >= 0 && "Non Block failed");

    if (nonblock)
        flag = ::fcntl(sock, F_SETFL, flag | O_NONBLOCK);
    else
        flag = ::fcntl(sock, F_SETFL, flag & ~O_NONBLOCK);
}

void SetNodelay(int sock)
{
    int nodelay = 1;
    ::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(int));
}

void SetSndBuf(int sock, socklen_t winsize)
{
    ::setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&winsize, sizeof(winsize));
}

void SetRcvBuf(int sock, socklen_t winsize)
{
    ::setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&winsize, sizeof(winsize));
}

void SetReuseAddr(int sock)
{
    int reuse = 1;
    ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
}

bool GetLocalAddr(int sock, SocketAddr& addr)
{
    sockaddr_in localAddr;
    socklen_t   len = sizeof(localAddr);

    if (0 == ::getsockname(sock, (struct sockaddr*)&localAddr, &len))
    {
        addr.Init(localAddr);
    }
    else
    {
        return  false;
    }

    return  true;
}

bool GetPeerAddr(int sock, SocketAddr& addr)
{
    sockaddr_in  remoteAddr;
    socklen_t    len = sizeof(remoteAddr);
    if (0 == ::getpeername(sock, (struct sockaddr*)&remoteAddr, &len))
    {
        addr.Init(remoteAddr);
    }
    else
    {
        return  false;
    }

    return  true;
}

in_addr_t GetLocalAddrInfo()
{
#ifdef __APPLE__
    // TODO can not work on mac os
    return 0;
#else
    char buff[512];
    struct ifconf conf;
    conf.ifc_len = sizeof buff;
    conf.ifc_buf = buff;

    int sock = CreateUDPSocket();
    ioctl(sock, SIOCGIFCONF, &conf);
    int maxnum  = conf.ifc_len / sizeof(struct ifreq);
    struct ifreq* ifr = conf.ifc_req;

    in_addr_t result = 0;
    for (int i = 0; i < maxnum; i++)
    {
        if (!ifr)
            break;

        ioctl(sock, SIOCGIFFLAGS, ifr);

        if (((ifr->ifr_flags & IFF_LOOPBACK) == 0) && (ifr->ifr_flags & IFF_UP))
        {
            struct sockaddr_in *pAddr = (struct sockaddr_in *)(&ifr->ifr_addr);
            result = pAddr->sin_addr.s_addr;
            break;
        }
        ++ ifr;
    }

    ::close(sock);
    return result;
#endif
}

rlim_t GetMaxOpenFd()
{
    struct rlimit rlp;

    if (getrlimit(RLIMIT_NOFILE, &rlp) == 0)
        return rlp.rlim_cur;

    return 0;
}

bool SetMaxOpenFd(rlim_t maxfdPlus1)
{
    struct rlimit rlp;

    if (getrlimit(RLIMIT_NOFILE, &rlp) != 0)
        return false;

    if (maxfdPlus1 <= rlp.rlim_cur)
        return true;

    if (maxfdPlus1 > rlp.rlim_max)
        return false;

    rlp.rlim_cur = maxfdPlus1;

    return setrlimit(RLIMIT_NOFILE, &rlp) == 0;
}

std::string ConvertIp(const char* ip)
{
    if (strncmp(ip, "loopback", 8) == 0)
        return "127.0.0.1";

    if (strncmp(ip, "localhost", 9) == 0)
    {
        ananas::SocketAddr tmp;
        tmp.Init(ananas::GetLocalAddrInfo(), 0);
        return tmp.GetIP();
    }

    return ip;
}

} // end namespace ananas

