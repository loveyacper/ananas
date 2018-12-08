#include <algorithm>
#include <iostream>
#include <cassert>
#include "RedisLog.h"
#include "RedisContext.h"

#define CRLF "\r\n"


static
ananas::PacketLen_t ProcessInlineCmd(const char* buf,
                                     size_t bytes,
                                     std::vector<std::string>& params) {
    if (bytes < 2)
        return 0;

    std::string res;

    for (size_t i = 0; i + 1 < bytes; ++ i) {
        if (buf[i] == '\r' && buf[i+1] == '\n') {
            if (!res.empty())
                params.emplace_back(std::move(res));

            return static_cast<ananas::PacketLen_t>(i + 2);
        }

        if (isblank(buf[i])) {
            if (!res.empty()) {
                params.reserve(4);
                params.emplace_back(std::move(res));
            }
        } else {
            res.reserve(16);
            res.push_back(buf[i]);
        }
    }

    return 0;
}


static thread_local DB* g_db;


RedisContext::RedisContext(ananas::Connection* conn) :
    hostConn_(conn) {
}


ananas::PacketLen_t RedisContext::OnRecv(ananas::Connection* conn, const char* data, ananas::PacketLen_t len) {
    const char* const end = data + len;
    const char* ptr = data;

    auto parseRet = proto_.ParseRequest(ptr, end);
    if (parseRet == ParseResult::error) {
        if (!proto_.IsInitialState()) {
            ERR(logger) << "ParseError for " << data;
            hostConn_->ActiveClose();
            return 0;
        }
        // try inline cmd
        std::vector<std::string> params;
        auto l = ::ProcessInlineCmd(ptr, len, params);
        if (l == 0)
            return 0;

        ptr += l;
        proto_.SetParams(std::move(params));
        parseRet = ParseResult::ok;
    }

    if (parseRet != ParseResult::ok) {
        // wait
        return static_cast<ananas::PacketLen_t>(ptr - data);
    }

    // handle packet
    const auto& params = proto_.GetParams();
    if (params.empty())
        return static_cast<ananas::PacketLen_t>(ptr - data);

    std::string cmd(params[0]);
    std::transform(params[0].begin(), params[0].end(), cmd.begin(), ::tolower);

    const char* reply = nullptr;
    size_t replyBytes = 0;

    if (cmd == "ping") {
        reply = "+PONG" CRLF;
        replyBytes = 7;
    } else if (cmd == "get") {
        reply_ = _Get(params[1]);

        reply = reply_.data();
        replyBytes = reply_.size();
    } else if (cmd == "set") {
        _Set(params[1], params[2]);
        reply = "+OK" CRLF;
        replyBytes = 5;
    } else {
        ERR(logger) << "Unknown cmd " << cmd;

        reply = "-ERR Unknown command\r\n";
        replyBytes = sizeof "-ERR Unknown command\r\n" - 1;
    }

    proto_.Reset();
    hostConn_->SendPacket(reply, replyBytes);
    return static_cast<ananas::PacketLen_t>(ptr - data);
}

// helper
static size_t FormatBulk(const char* str, size_t len, std::string* reply) {
    assert (reply);

    size_t oldSize = reply->size();
    (*reply) += '$';

    char val[32];
    int tmp = snprintf(val, sizeof val - 1, "%lu" CRLF, len);
    reply->append(val, tmp);

    if (str && len > 0)
        reply->append(str, len);

    reply->append(CRLF, 2);
    return reply->size() - oldSize;
}

std::string RedisContext::_Get(const std::string& key) {
    if (!g_db)
        g_db = new DB();

    auto it = (*g_db).find(key);
    if (it == (*g_db).end())
        return "$-1" CRLF;

    const auto& val = it->second;
    std::string res;
    res.reserve(2 * val.size());
    FormatBulk(val.data(), val.size(), &res);

    return res;
}

void RedisContext::_Set(const std::string& key, const std::string& val) {
    if (!g_db)
        g_db = new DB();
    (*g_db)[key] = val;
}

