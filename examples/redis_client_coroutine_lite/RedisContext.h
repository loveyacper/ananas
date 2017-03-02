#ifndef BERT_REDISCONTEXT_H
#define BERT_REDISCONTEXT_H

#include <queue>
#include <string>
#include <vector>
#include "net/Connection.h"
#include "coroutine/Coroutine.h"

#define CRLF "\r\n"

// helpers
const char* Strstr(const char* ptr, size_t nBytes, const char* pattern, size_t nBytes2);
const char* SearchCRLF(const char* ptr, size_t nBytes);

enum ResponseType
{
    None,
    Fine,
    Error,
    String,
};


// Build redis request from multiple strings, use inline protocol 
template <typename... Args>
std::string BuildRedisRequest(Args&& ...);

template <typename S>
std::string BuildRedisRequest(S&& s)
{
    return std::string(std::forward<S>(s)) + "\r\n";
}

template <typename H, typename... T>
std::string BuildRedisRequest(H&& head, T&&... tails)
{
    std::string h(std::forward<H>(head));
    return h + " " + BuildRedisRequest(std::forward<T>(tails)...);
}

class RedisContext
{
public:
    explicit
    RedisContext(ananas::Connection* conn);

    template <typename F, typename... Args>
    bool StartCoroutine(F&& f, Args&&... args)
    {
        current_ = s_crtMgr.CreateCoroutine(std::forward<F>(f), std::forward<Args>(args)...);
        auto result = s_crtMgr.Send(current_); // prime the coroutine
        if (!result)
        {
            s_crtMgr.Send(current_);
            return false;
        }

        return true;
    }

    // Redis get command
    ananas::AnyPointer Get(const std::string& key);
    // Parse response
    ananas::PacketLen_t OnRecv(ananas::Connection* conn, const char* data, ananas::PacketLen_t len);

    static void PrintResponse(const std::pair<ResponseType, std::string>& info);

private:
    ananas::Connection* hostConn_;

    struct Request
    {
        std::vector<std::string> request;
        ananas::CoroutinePtr crt;

        Request(Request const& ) = delete;
        void operator= (Request const& ) = delete;

        Request() { }

        Request(Request&& r) :
            request(std::move(r.request)),
            crt(std::move(r.crt))
        {
        }

        Request& operator= (Request&& r) {
            if (this != &r) {
                request = std::move(r.request);
                crt = std::move(r.crt);
            }

            return *this;
        }
    };

    std::queue<Request> pending_;

    // only support +OK\r\n, -Err\r\n, $len\r\ncontent\r\n
    ResponseType type_;
    std::string content_;
    int len_ = -1;

    void _ResetResponse();
    
    ananas::CoroutinePtr current_; 
    static ananas::CoroutineMgr s_crtMgr; 
};

#endif

