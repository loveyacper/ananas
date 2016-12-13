#include <algorithm>
#include <iostream>
#include "RedisContext.h"

#define CRLF "\r\n"

const char* Strstr(const char* ptr, size_t nBytes, const char* pattern, size_t nBytes2)
{
    if (!pattern || *pattern == 0)
        return nullptr;
            
    const char* ret = std::search(ptr, ptr + nBytes, pattern, pattern + nBytes2);
    return  ret == ptr + nBytes ? nullptr : ret;
}

const char* SearchCRLF(const char* ptr, size_t nBytes)
{
    return Strstr(ptr, nBytes, CRLF, 2);
}

RedisContext::RedisContext(ananas::Connection* conn) : hostConn_(conn)
{
    _ResetResponse();
}

    
ananas::Future<std::pair<ResponseType, std::string> >
RedisContext::Get(const std::string& key)
{
    // Redis inline protocol request
    std::string req_buf = BuildRedisRequest("get", key);
    hostConn_->SendPacket(req_buf.data(), req_buf.size());

    RedisContext::Request req;
    req.request.push_back("get");
    req.request.push_back(key);

    auto fut = req.promise.GetFuture();
    pending_.push(std::move(req));

    return fut;
}
    
ananas::Future<std::pair<ResponseType, std::string> >
RedisContext::Set(const std::string& key, const std::string& value)
{
    // Redis inline protocol request
    std::string req_buf = BuildRedisRequest("set", key, value);
    hostConn_->SendPacket(req_buf.data(), req_buf.size());

    RedisContext::Request req;
    req.request.push_back("set");
    req.request.push_back(key);
    req.request.push_back(value);

    auto fut = req.promise.GetFuture();
    pending_.push(std::move(req));

    return fut;
}


ananas::PacketLen_t RedisContext::OnRecv(ananas::Connection* conn, const char* data, ananas::PacketLen_t len)
{
    // just for test.
    if (type_ == None)
    {
        switch (data[0])
        {
            case '+':
                type_ = Fine;
                break;

            case '-':
                type_ = Error;
                break;

            case '$':
                type_ = String;
                break;

            default:
                assert (!!!"wrong type");
                break;
        }
    }

    const char* res = SearchCRLF(data, len);
    if (!res)
        return 0;  // waiting

    bool ready = false;
    switch (type_)
    {
        case Fine:
            content_.assign(data + 1, res - (data + 1));
            ready = true;
            break;

        case Error:
            content_.assign(data + 1, res - (data + 1));
            ready = true;
            break;

        case String:
            if (len_ == -1)
            {
                std::string number(data + 1, res - (data + 1));
                len_ = std::stoi(number);
                if (len_ == -1)
                {
                    content_ = "(nil)";
                    len_ = 5;
                    ready = true;
                }
            }
            else
            {
                content_.assign(data, res - data);
                assert (content_.size() == len_);
                ready = true;
            }
            break;

        default:
            break;
    }

    if (ready)
    {
        auto& req = pending_.front();
        std::cout << "--- Request: [ ";
        for (const auto& arg : req.request)
            std::cout << arg << " ";
        std::cout << "]\n--- Response: ";

        req.promise.SetValue(std::make_pair(type_, content_));
        pending_.pop();

        _ResetResponse();
    }
                
    return res - data + 2;
}

    
    
void RedisContext::PrintResponse(const std::pair<ResponseType, std::string>& info)
{
    if (info.first == Error)
        std::cout << "(Error): " << info.second << std::endl;
    else if (info.first == Fine)
        std::cout << "(Fine): " << info.second << std::endl;
    else if (info.first == String)
        std::cout << "(String): " << info.second << std::endl; 
    else 
        assert(false);
}

void RedisContext::_ResetResponse()
{
    type_ = None;
    content_.clear();
    len_ = -1;
}

#if 0
void OnConnect(ananas::Connection* conn)
{
        std::cout << "RedisContext::OnConnect " << conn->Identifier() << std::endl;
#if 1
        // issue request
        this->Get("name").Then([](const std::pair<ResponseType, std::string>& info) {
                if (info.first == Error) {
                    std::cout << "Error:" << info.second << std::endl;
                }else if (info.first == Fine) {
                    std::cout << "Fine:" << info.second << std::endl;
                } else if (info.first == String) {
                    std::cout << "Content:" << info.second << std::endl;
                } else {
                    assert(false);
                }
            });
#endif
    }

private:
    ananas::Connection* hostConn_;
};
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

