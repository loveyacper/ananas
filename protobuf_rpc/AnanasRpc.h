#ifndef BERT_ANANASRPC_H
#define BERT_ANANASRPC_H

#include <memory>
#include <atomic>
#include <string>
#include <unordered_map>

#include <google/protobuf/service.h>

#include "net/Typedefs.h"

namespace google
{
namespace protobuf
{

class Message;
class Closure;
class MethodDescriptor;

}
}

namespace ananas
{

class Connection;

namespace rpc
{

class Request;
class Response;
class RpcMessage;

class RpcChannel;

class RpcService
{
public:
    RpcService();
    ~RpcService();


    bool AddService(std::unique_ptr<google::protobuf::Service>&& service);
    bool AddService(google::protobuf::Service* service);

    google::protobuf::Service* GetGenericService(const std::string& name) const;

    // 1.client发起:先找到service然后cast成自定义类型，构造stub，传入channel再调用感兴趣方法
    template <typename T, typename = typename T::Stub>
    std::unique_ptr<typename T::Stub> GetServiceStub(const std::string& name) const;
    
    void OnNewConnection(ananas::Connection* conn);

    void SetOnCreateChannel(std::function<void (RpcChannel* )> );

private:
    void _OnDisconnect(ananas::Connection* conn);

    // TODO 一个service类型，对应多个channel: load balance
    RpcChannel* GetChannelByService(const std::string& name) const;

    using Table = std::unordered_map<std::string, std::unique_ptr<google::protobuf::Service> >;
    Table services_;

    // 实际应该一个service类型，对应多个channel
    std::unordered_map<unsigned int, std::unique_ptr<RpcChannel> > channels_;
    std::function<void (RpcChannel* )> onCreateChannel_;
};


template <typename T, typename >
inline std::unique_ptr<typename T::Stub> RpcService::GetServiceStub(const std::string& name) const
{
    static_assert(std::is_base_of<google::protobuf::Service, T>::value,
            "T must be google::protobuf::Service's subclass type");

    using STUB = typename T::Stub;

    auto serv = GetGenericService(name);
    if (!serv)
        return std::unique_ptr<STUB>(nullptr);

    return std::unique_ptr<STUB>(new STUB(GetChannelByService(name)));
}


// 和connection 1:1对应
// TODO timeout
class RpcChannel : public google::protobuf::RpcChannel
{
    friend class RpcService;
public:
    RpcChannel(ananas::Connection* conn, RpcService* service);
    ~RpcChannel();

    RpcService* Service() const {
        return rpcServices_;
    }


    // Call the given method of the remote service.  The signature of this
    // procedure looks the same as Service::CallMethod(), but the requirements
    // are less strict in one important way:  the request and response objects
    // need not be of any specific class as long as their descriptors are
    // method->input_type() and method->output_type().
    void CallMethod(const google::protobuf::MethodDescriptor* method,
            google::protobuf::RpcController* ,  // per request, but no use for now
            const google::protobuf::Message* request, 
            google::protobuf::Message* response, 
            google::protobuf::Closure* done) override final;

private:
    int _TotalLength(const char* data) const;

    ananas::PacketLen_t _OnMessage(ananas::Connection* conn, const char* data, size_t len);

    // server-side, send response
    void _OnServDone(int id, std::shared_ptr<google::protobuf::Message> response);
    void _OnServError(int id, const std::string& error);
    // server-side, process request
    void _ProcessRequest(const ananas::rpc::Request& req);
    // client-side
    void _ProcessResponse(const ananas::rpc::Response& rsp);

    ananas::Connection* const conn_;
    RpcService* const rpcServices_;

    using PendingItem = std::pair<google::protobuf::Message*,
                                  google::protobuf::Closure*>;
    std::unordered_map<int, PendingItem> pendingCalls_; // id-> response + done_closure

    static std::atomic<int> s_idGen;
    static const int kHeaderLen;
};

} // end namespace rpc

} // end namespace ananas

#endif

