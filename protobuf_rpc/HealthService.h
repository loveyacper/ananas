#ifndef BERT_HEALTHSERVICE_H
#define BERT_HEALTHSERVICE_H

#include <string>
#include "protobuf_rpc/ananas_rpc.pb.h"

namespace google {
namespace protobuf {

class Message;

}
}

namespace ananas {

namespace rpc {

class ServerChannel;

class HealthServiceImpl : public HealthHttpService {
public:
    void GetSummary(::google::protobuf::RpcController*,
                    const rpc::HttpRequestMsg* request,
                    rpc::Summary* response,
                    ::google::protobuf::Closure* done) override final;
};

extern void OnCreateHealthChannel(rpc::ServerChannel* c);
extern std::string DispatchHealthMethod(const ::google::protobuf::Message* m);

} // end namespace rpc

} // end namespace ananas

#endif

