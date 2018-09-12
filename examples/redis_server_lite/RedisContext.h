#ifndef BERT_REDISCONTEXT_H
#define BERT_REDISCONTEXT_H

#include <queue>
#include <string>
#include <vector>
#include <unordered_map>

#include "net/Connection.h"
#include "Protocol.h"

using DB = std::unordered_map<std::string, std::string>;

//extern DB g_db;

class RedisContext {
public:
    explicit
    RedisContext(ananas::Connection* conn);

    ananas::PacketLen_t OnRecv(ananas::Connection* conn, const char* data, ananas::PacketLen_t len);

private:
    std::string _Get(const std::string& key);
    void _Set(const std::string& key, const std::string& val);

    Protocol proto_;
    ananas::Connection* hostConn_;
    std::string reply_;
};

#endif

