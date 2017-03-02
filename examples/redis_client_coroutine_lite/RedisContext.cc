#include <algorithm>
#include <iostream>
#include <cassert>
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

ananas::CoroutineMgr RedisContext::s_crtMgr; 

RedisContext::RedisContext(ananas::Connection* conn) : hostConn_(conn)
{
    _ResetResponse();
}

ananas::AnyPointer RedisContext::Get(const std::string& key)
{
    // Redis inline protocol request
    std::string req_buf = BuildRedisRequest("get", key);
    if (!hostConn_->SendPacket(req_buf.data(), req_buf.size()))
        return ananas::AnyPointer();

    RedisContext::Request req;
    req.request.push_back("get");
    req.request.push_back(key);

    req.crt = current_;
    pending_.push(std::move(req));

    return ananas::AnyPointer((int*)0x1, [] (int* ) {});
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
                assert ((int)(content_.size()) == len_);
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

        s_crtMgr.Send(req.crt, std::make_shared<std::string>(content_));
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

