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

    // Redis get command
    ananas::Future<std::pair<ResponseType, std::string> > Get(const std::string& key);
    // Redis set command
    ananas::Future<std::pair<ResponseType, std::string> > Set(const std::string& key, const std::string& value);
    // Parse response
    ananas::PacketLen_t OnRecv(ananas::Connection* conn, const char* data, ananas::PacketLen_t len);

    static void PrintResponse(const std::pair<ResponseType, std::string>& info);

private:
    ananas::Connection* hostConn_;

    struct Request
    {
        std::vector<std::string> request;
        ananas::Promise<std::pair<ResponseType, std::string> > promise;

        Request(Request const& ) = delete;
        void operator= (Request const& ) = delete;

        Request() { }

        Request(Request&& r) {
            request = std::move(r.request);
            promise = std::move(r.promise);
        }

        Request& operator= (Request&& r) {
            if (&r != this) {
                request = std::move(r.request);
                promise = std::move(r.promise);
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
};

#if 0
// for test
void OnNewConnection(ananas::Connection* conn)
{
    std::cout << "OnNewConnection " << conn->Identifier() << std::endl;

    std::shared_ptr<RedisContext> ctx = std::make_shared<RedisContext>(conn);

    conn->SetOnConnect(std::bind(&RedisContext::OnConnect, ctx, std::placeholders::_1));
    conn->SetOnMessage(std::bind(&RedisContext::OnRecv, ctx, std::placeholders::_1,
                                                             std::placeholders::_2,
                                                             std::placeholders::_3));
}

    
void OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer)
{
    std::cout << "OnConnFail " << peer.GetPort() << std::endl;

    // reconnect
    loop->ScheduleAfter(std::chrono::seconds(2), [=]() {
        loop->Connect(peer, OnNewConnection, OnConnFail);
    });
}

int main()
{
    ananas::EventLoop loop;
    loop.Connect("loopback", 6379, OnNewConnection, OnConnFail);

    loop.Run();

    return 0;
}
#endif

#endif

