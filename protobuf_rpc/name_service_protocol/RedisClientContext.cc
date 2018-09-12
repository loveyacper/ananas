#include <ctype.h>
#include <string>
#include <algorithm>

#include "RedisProtocol.h"
#include "RedisClientContext.h"

#include "protobuf_rpc/ananas_rpc.pb.h"
#include "protobuf_rpc/RpcServiceStub.h"
#include "util/Util.h"

namespace ananas {

namespace rpc {

// hset -> keepalive
// hgetall -> getendpoints

bool RedisClientContext::M2FEncode(const google::protobuf::Message* msg, ananas::rpc::RpcMessage& frame) {
    auto request = frame.mutable_request();
    std::string* result = request->mutable_serialized_request();
    auto req = dynamic_cast<const ananas::rpc::ServiceName*>(msg);
    if (req) {
        // GetEndpoints
        *result = "hgetall " + req->name();
        operates_.push(Oper::GET_ENDPOINTS);
    } else {
        auto req = dynamic_cast<const ananas::rpc::KeepaliveInfo*>(msg);
        if (!req)
            return false;

        // Keepalive
        *result = "hset " + req->servicename() + " " +
                  EndpointToString(req->endpoint()) + " " +
                  std::to_string(time(nullptr));
        operates_.push(Oper::KEEPALIVE);
    }

    *result += "\r\n";
    return true;
}

std::shared_ptr<google::protobuf::Message> RedisClientContext::B2MDecode(const char*& data, size_t len) {
    auto state = proto_.Parse(data, data + len);
    if (state == ParseResult::ok) {
        ANANAS_DEFER {
            operates_.pop();
            proto_.Reset();
        };

        switch (operates_.front()) {
        case Oper::GET_ENDPOINTS: {
            time_t now = time(nullptr);
            auto msg = std::make_shared<ananas::rpc::EndpointList>();
            // hgetall ep time ep time
            for (auto it(proto_.params_.begin()); it != proto_.params_.end(); ) {
                Endpoint ep = EndpointFromString(*it);
                ++it;
                time_t t = std::stoi(*it ++);
                // TODO kick timeout
                if (t + 30 > now)
                    *msg->add_endpoints() = ep;
            }

            return std::static_pointer_cast<ananas::rpc::EndpointList>(msg);
        }
        case Oper::KEEPALIVE: {
            auto msg = std::make_shared<ananas::rpc::Status>();
            msg->set_result(0);
            return std::static_pointer_cast<google::protobuf::Message>(msg);
        }
        break;
        }
        return std::shared_ptr<google::protobuf::Message>();
    } else if (state == ParseResult::wait) {
        return std::shared_ptr<google::protobuf::Message>();
    }

    return std::shared_ptr<google::protobuf::Message>();
}

void OnCreateRedisChannel(ananas::rpc::ClientChannel* chan) {
    auto ctx = std::make_shared<RedisClientContext>();
    chan->SetContext(ctx);

    ananas::rpc::Encoder encoder;
    encoder.SetMessageToFrameEncoder(std::bind(&RedisClientContext::M2FEncode,
                                     ctx.get(),
                                     std::placeholders::_1,
                                     std::placeholders::_2));
    chan->SetEncoder(std::move(encoder));

    ananas::rpc::Decoder decoder;
    decoder.SetBytesToMessageDecoder(std::bind(&RedisClientContext::B2MDecode,
                                     ctx.get(),
                                     std::placeholders::_1,
                                     std::placeholders::_2));
    chan->SetDecoder(std::move(decoder));
}

} // end namespace rpc

} // end namespace ananas

