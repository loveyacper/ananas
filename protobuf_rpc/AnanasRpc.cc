#include <google/protobuf/descriptor.h>

#include "AnanasRpc.h"
#include "ananas_rpc.pb.h"
#include "net/Connection.h"
#include "util/Util.h"

namespace ananas
{

namespace rpc
{

RpcService::RpcService()
{
}

RpcService::~RpcService()
{
}

bool RpcService::AddService(std::unique_ptr<google::protobuf::Service>&& service)
{
    auto name = service->GetDescriptor()->full_name(); // copy name
    return services_.insert(std::make_pair(name, std::move(service))).second;
}

bool RpcService::AddService(google::protobuf::Service* service)
{
    const auto& name = service->GetDescriptor()->full_name();
    return services_.insert(std::make_pair(name, std::unique_ptr<google::protobuf::Service>(service))).second;
}
    
google::protobuf::Service* RpcService::GetGenericService(const std::string& name) const
{
    auto it = services_.find(name);
    if (it == services_.end())
        return nullptr;

    return it->second.get();
}

void RpcService::OnNewConnection(ananas::Connection* conn)
{
    RpcChannel* channel = new RpcChannel(conn, this);
    channels_.insert(std::make_pair(conn->GetUniqueId(), std::unique_ptr<RpcChannel>(channel)));

    conn->SetOnDisconnect(std::bind(&RpcService::_OnDisconnect, this, std::placeholders::_1));
    conn->SetOnMessage(std::bind(&RpcChannel::_OnMessage, channel, std::placeholders::_1,
                                                                   std::placeholders::_2,
                                                                   std::placeholders::_3));

    if (onCreateChannel_)
        onCreateChannel_(channel);
}

void RpcService::SetOnCreateChannel(std::function<void (RpcChannel* )> cb)
{
    onCreateChannel_ = std::move(cb);
}

RpcChannel* RpcService::GetChannelByService(const std::string& name) const
{
    if (channels_.empty())
        return nullptr;

    return channels_.begin()->second.get();
}

void RpcService::_OnDisconnect(ananas::Connection* conn)
{
    channels_.erase(conn->GetUniqueId());
}

    

std::atomic<int> RpcChannel::s_idGen{0};
const int RpcChannel::kHeaderLen = 4;

RpcChannel::RpcChannel(ananas::Connection* conn, RpcService* services) :
    conn_(conn),
    rpcServices_(services)
{
}

RpcChannel::~RpcChannel()
{
    // TODO delete pendingCalls
}

// The client stub's concrete method will call me
void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                            google::protobuf::RpcController* ,
                            const google::protobuf::Message* request, 
                            google::protobuf::Message* response, 
                            google::protobuf::Closure* done)
{
    RpcMessage rpcMsg;

    Request& req = *rpcMsg.mutable_request();
    req.set_id(++ s_idGen);
    req.set_service_name(method->service()->full_name());
    req.set_method_name(method->name());

    request->SerializeToString(req.mutable_serialized_request());
    delete request;

    const int bodyLen = rpcMsg.ByteSize();
    const int totalLen = kHeaderLen + bodyLen;
    std::unique_ptr<char []> bytes(new char[bodyLen]);
    rpcMsg.SerializeToArray(bytes.get(), bodyLen);

    // format: 4 byte length, 
    ananas::SliceVector vslice;
    vslice.PushBack(&totalLen, sizeof totalLen);
    vslice.PushBack(bytes.get(), bodyLen);

    conn_->SendPacket(vslice);
    printf("Send request %d\n", req.id());

    if (response) 
    {
        //add to pending_call;
        PendingItem item{response, done};
        pendingCalls_.insert({req.id(), item});
    }
    else
        ; // one-way rpc
}


// server call this, send response
void RpcChannel::_OnServDone(int id, google::protobuf::Message* response)
{
    printf("_OnServDone id %d\n", id);

    RpcMessage rpcMsg;
    Response& rsp = *rpcMsg.mutable_response();

    rsp.set_id(id);
    response->SerializeToString(rsp.mutable_serialized_response());
    delete response;

    const int bodyLen = rpcMsg.ByteSize();
    const int totalLen = kHeaderLen + bodyLen;
    std::unique_ptr<char []> bytes(new char[bodyLen]);
    rpcMsg.SerializeToArray(bytes.get(), bodyLen);

    // format: 4 byte length, 
    ananas::SliceVector vslice;
    vslice.PushBack(&totalLen, sizeof totalLen);
    vslice.PushBack(bytes.get(), bodyLen);

    conn_->SendPacket(vslice);
}


void RpcChannel::_OnServError(int id, const std::string& error)
{
    printf("_OnServError id %d\n", id);

    RpcMessage rpcMsg;
    Response& rsp = *rpcMsg.mutable_response();

    rsp.set_id(id);
    rsp.set_error(error);

    const int bodyLen = rpcMsg.ByteSize();
    const int totalLen = kHeaderLen + bodyLen;
    std::unique_ptr<char []> bytes(new char[bodyLen]);
    rpcMsg.SerializeToArray(bytes.get(), bodyLen);

    // format: 4 byte length, 
    ananas::SliceVector vslice;
    vslice.PushBack(&totalLen, sizeof totalLen);
    vslice.PushBack(bytes.get(), bodyLen);

    conn_->SendPacket(vslice);
}

int RpcChannel::_TotalLength(const char* data) const
{
    int len;
    memcpy(&len, data, kHeaderLen); // no big endian

    return len;
}


size_t RpcChannel::_OnMessage(ananas::Connection* conn, const char* data, size_t len)
{
    assert (conn == conn_);

    if (len <= kHeaderLen)
        return 0;

    int totalLen = _TotalLength(data);
    if (totalLen <= kHeaderLen || totalLen >= 100 * 10 * 1024)
    {
        // evil client
        conn->Close();
        return 0;
    }

    if (static_cast<int>(len) < totalLen)
        return 0; // not complete

    data += kHeaderLen;

    RpcMessage msg;
    msg.ParseFromArray(data, totalLen - kHeaderLen);

    if (msg.has_request())
        _ProcessRequest(msg.request());
    else if (msg.has_response())
        _ProcessResponse(msg.response());
    else
        // evil client
        conn->Close();

    return totalLen;
}

// server-side
void RpcChannel::_ProcessRequest(const ananas::rpc::Request& req)
{
    std::string error;

    ANANAS_DEFER
    {
        if (!error.empty()) 
            this->_OnServError(req.id(), error);
    };

    if (!req.has_service_name())
    {
        error = "No service name found in request.";
        return;
    }

    if (!req.has_method_name())
    {
        error = "No method name found in request.";
        return;
    }

    auto service = rpcServices_->GetGenericService(req.service_name());
    if (!service)
    {
        error = "No such service";
        return;
    }

    auto method = service->GetDescriptor()->FindMethodByName(req.method_name());
    if (!method)
    {
        error = "No such method";
        return;
    }

    //auto request = std::make_shared<google::protobuf::Message>(service->GetRequestPrototype(method).New()); 
    auto request = service->GetRequestPrototype(method).New(); 
    request->ParseFromString(req.serialized_request());

    auto response = service->GetResponsePrototype(method).New(); 

    const int id = req.id();
    printf("serv for request id %d\n", id);
    service->CallMethod(method, nullptr, request, response,
            NewCallback(this, &RpcChannel::_OnServDone, id, response));
}

// client-side
void RpcChannel::_ProcessResponse(const ananas::rpc::Response& rsp)
{
    int id = rsp.id();

    PendingItem item{nullptr, nullptr};
    auto it = pendingCalls_.find(id);
    if (it != pendingCalls_.end())
    {
        item = it->second;
        pendingCalls_.erase(it);
    }

    // check error
    if (rsp.has_error())
    {
        printf("%d has error %s\n", id, rsp.error().data());
        return;
    }

    assert (rsp.has_serialized_response());

    google::protobuf::Message* response = item.first;
    if (response)
    {
        std::unique_ptr<google::protobuf::Message> ptr(response);
        if (response->ParseFromString(rsp.serialized_response()))
        {
            printf("response->ParseFromString succ\n");
            if (item.second)
                item.second->Run();
        }
        else
            printf("response->ParseFromString failed\n");
    }
    else
    {
        // one way rpc
    }
}

} // end namespace rpc

} // end namespace ananas

