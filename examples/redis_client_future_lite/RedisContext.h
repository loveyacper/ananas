#ifndef BERT_REDISCONTEXT_H
#define BERT_REDISCONTEXT_H

#include <queue>
#include <string>
#include <vector>
#include "net/Connection.h"
#include "future/Future.h"

#define CRLF "\r\n"

// helpers
const char* Strstr(const char* ptr, size_t nBytes, const char* pattern, size_t nBytes2);
const char* SearchCRLF(const char* ptr, size_t nBytes);

enum ResponseType {
    None,
    Fine,
    Error,
    String,
};


// Build redis request from multiple strings, use inline protocol
template <typename... Args>
std::string BuildRedisRequest(Args&& ...);

template <typename S>
std::string BuildRedisRequest(S&& s) {
    return std::string(std::forward<S>(s)) + "\r\n";
}

template <typename H, typename... T>
std::string BuildRedisRequest(H&& head, T&&... tails) {
    std::string h(std::forward<H>(head));
    return h + " " + BuildRedisRequest(std::forward<T>(tails)...);
}

class RedisContext {
public:
    explicit
    RedisContext(ananas::Connection* conn);

    // Redis get command
    ananas::Future<std::pair<ResponseType, std::string> > Get(const std::string& key);
    // Redis set command
    ananas::Future<std::pair<ResponseType, std::string> > Set(const std::string& key, const std::string& value);
    // Parse response
    size_t OnRecv(ananas::Connection* conn, const char* data, size_t len);

    static void PrintResponse(const std::pair<ResponseType, std::string>& info);

private:
    ananas::Connection* hostConn_;

    struct Request {
        std::vector<std::string> request;
        ananas::Promise<std::pair<ResponseType, std::string> > promise;

        Request(Request const& ) = delete;
        void operator= (Request const& ) = delete;

        Request() { }

        Request(Request&& ) = default;
        Request& operator= (Request&& ) = default;
    };

    std::queue<Request> pending_;

    // only support +OK\r\n, -Err\r\n, $len\r\ncontent\r\n
    ResponseType type_;
    std::string content_;
    int len_ = -1;

    void _ResetResponse();
};

#endif

