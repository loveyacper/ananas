#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "RpcService.h"
#include "RpcClosure.h"
#include "ProtobufCoder.h"
#include "ananas_rpc.pb.h"
#include "net/Connection.h"
#include "net/Application.h"
#include "util/Util.h"

namespace ananas
{

namespace rpc
{

Service::Service(google::protobuf::Service* service) :
    minLen_(0)
{
    service_.reset(service);
    name_ = service->GetDescriptor()->full_name();
}

Service::~Service()
{
}

google::protobuf::Service* Service::GetService() const
{
    return service_.get();
}

void Service::SetBindAddr(const SocketAddr& addr)
{
    assert (!bindAddr_.IsValid());
    bindAddr_ = addr;
}

bool Service::Start()
{
    if (!bindAddr_.IsValid())
        return false;

    auto& app = ananas::Application::Instance();
    app.Listen(bindAddr_, std::bind(&Service::OnNewConnection, this, std::placeholders::_1));
    return true;
}

const std::string& Service::FullName() const
{
    return name_;
}

void Service::SetOnMessage(TcpMessageCallback cb)
{
    handleMsg_ = std::move(cb);
}

void Service::OnNewConnection(ananas::Connection* conn)
{
    auto channel = std::make_shared<ananas::rpc::ServerChannel>(conn, this);
    conn->SetUserData(channel);

    assert (conn->GetLoop()->Id() < channels_.size());

    auto& channelMap = channels_[conn->GetLoop()->Id()];
    bool succ = channelMap.insert({conn->GetUniqueId(), channel.get()}).second;
    assert (succ);

    conn->SetOnDisconnect(std::bind(&Service::_OnDisconnect, this, std::placeholders::_1));

    if (handleMsg_)
    {
        conn->SetMinPacketSize(minLen_);
        conn->SetOnMessage(handleMsg_);
    }
    else
    {
        conn->SetMinPacketSize(kPbHeaderLen);
        conn->SetOnMessage(std::bind(&Service::OnProtobufMessage,
                                     this,
                                     std::placeholders::_1,
                                     std::placeholders::_2,
                                     std::placeholders::_3));
    }
}

void Service::OnRegister()
{
    channels_.resize(Application::Instance().NumOfWorker());
}

size_t Service::OnProtobufMessage(ananas::Connection* conn, const char* data, size_t len)
{
    assert (len >= kPbHeaderLen);

    const char* const start = data;

    RpcMessage msg;
    const auto state = BytesToPBFrameDecoder(data, len, msg);
    switch (state)
    {
        case DecodeState::eS_Error:
            // evil client
            conn->ActiveClose();
            return 0;

        case DecodeState::eS_Waiting:
            return static_cast<size_t>(data - start);

        case DecodeState::eS_Ok:
            break;

        default:
            assert (false);
    }

    if (msg.has_request())
        _ProcessRequest(conn, msg.request());
    else
        conn->ActiveClose(); // evil client

    return static_cast<size_t>(data - start);
}

// server-side
void Service::_ProcessRequest(ananas::Connection* conn, const ananas::rpc::Request& req)
{
    std::string error;
    int errnum = 0; // TODO

    ANANAS_DEFER
    {
        if (!error.empty()) 
            this->_OnServError(conn, req.id(), errnum, error);
    };

    if (req.service_name() != service_->GetDescriptor()->full_name())
    {
        error = "No such service:" + req.service_name();
        return;
    }

    auto method = service_->GetDescriptor()->FindMethodByName(req.method_name());
    if (!method)
    {
        error = "No such method:" + req.method_name();
        return;
    }

    /*
     * The resource manage is a little dirty, because CallMethod accepts raw pointers.
     * Here is my solution:
     * 1. request must be delete when exit this function, so if you want to process request
     * async, you MUST copy it.
     * 2. response is managed by Closure, so I use shared_ptr.
     * 3. Closure must be managed by raw pointer, so when call Closure::Run, it will execute
     * `delete this` when exit, at which time the response is also destroyed.
     */
    std::unique_ptr<google::protobuf::Message> request(service_->GetRequestPrototype(method).New()); 
    if (!request->ParseFromString(req.serialized_request()))
    {
        error = "Request invalid";
        return;
    }

    std::shared_ptr<google::protobuf::Message> response(service_->GetResponsePrototype(method).New()); 

    const int id = req.id();
    std::weak_ptr<ananas::Connection> wconn(std::static_pointer_cast<ananas::Connection>(conn->shared_from_this()));
    service_->CallMethod(method, nullptr, request.get(), response.get(), 
            new ananas::rpc::Closure(&Service::_OnServDone, this, wconn, id, response));
}

void Service::_OnDisconnect(ananas::Connection* conn)
{
    auto& channelMap = channels_[conn->GetLoop()->Id()];
    bool succ = channelMap.erase(conn->GetUniqueId());
    assert (succ);
}

void Service::_OnServDone(std::weak_ptr<ananas::Connection> wconn, int id, std::shared_ptr<google::protobuf::Message> response)
{
    auto conn = wconn.lock();
    if (!conn)
        return;

    RpcMessage rpcMsg;
    Response& rsp = *rpcMsg.mutable_response();

    rsp.set_id(id);
    response->SerializeToString(rsp.mutable_serialized_response());

    const int bodyLen = rpcMsg.ByteSize();
    const int totalLen = kPbHeaderLen + bodyLen;
    std::unique_ptr<char []> bytes(new char[bodyLen]);
    rpcMsg.SerializeToArray(bytes.get(), bodyLen);

    // format: 4 byte length, 
    ananas::SliceVector vslice;
    vslice.PushBack(&totalLen, sizeof totalLen);
    vslice.PushBack(bytes.get(), bodyLen);

    conn->SendPacket(vslice);
}

void Service::_OnServError(ananas::Connection* conn,
                           int id,
                           int errnum,
                           const std::string& errMsg)
{
    RpcMessage rpcMsg;
    Response& rsp = *rpcMsg.mutable_response();

    rsp.set_id(id);
    rsp.mutable_error()->set_errnum(errnum);
    rsp.mutable_error()->set_str(errMsg);

    const int bodyLen = rpcMsg.ByteSize();
    const int totalLen = kPbHeaderLen + bodyLen;
    std::unique_ptr<char []> bytes(new char[bodyLen]);
    rpcMsg.SerializeToArray(bytes.get(), bodyLen);

    // format: 4 byte length, 
    ananas::SliceVector vslice;
    vslice.PushBack(&totalLen, sizeof totalLen);
    vslice.PushBack(bytes.get(), bodyLen);

    conn->SendPacket(vslice);
}


ServerChannel::ServerChannel(ananas::Connection* conn, ananas::rpc::Service* service) :
    conn_(conn),
    service_(service)
{
}

ServerChannel::~ServerChannel()
{
}

ananas::rpc::Service* ServerChannel::Service() const
{
    return service_;
}

ananas::Connection* ServerChannel::Connection() const
{
    return conn_;
}

} // end namespace rpc

} // end namespace ananas

