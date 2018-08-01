#ifndef BERT_ANANAS_RPC_REDISCLIENTCONTEXT_H
#define BERT_ANANAS_RPC_REDISCLIENTCONTEXT_H

#include <memory>
#include <queue>
#include "RedisProtocol.h"

namespace google {
namespace protobuf {

class Message;
class Service;

}
}

namespace ananas {

namespace rpc {

class RpcMessage;
class ClientChannel;

class RedisClientContext {
public:
    bool M2FEncode(const google::protobuf::Message* msg, ananas::rpc::RpcMessage& frame);
    std::shared_ptr<google::protobuf::Message> B2MDecode(const char*& data, size_t len);

private:
    ClientProtocol proto_;

    enum class Oper {
        GET_ENDPOINTS,
        KEEPALIVE,
    };
    std::queue<Oper> operates_;
};

extern void OnCreateRedisChannel(ananas::rpc::ClientChannel* chan);

} // end namespace rpc

} // end namespace ananas

#endif

