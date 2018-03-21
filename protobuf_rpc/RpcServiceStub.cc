#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include <memory>
#include <string>
#include <vector>
#include <random>

#include "RpcServiceStub.h"
#include "net/Socket.h"
#include "net/Application.h"
#include "util/Util.h"

#include "ananas_rpc.pb.h"

namespace ananas
{

namespace rpc
{

ServiceStub::ServiceStub(google::protobuf::Service* service) :
    channels_(new ChannelMap())
{
    service_.reset(service);
    name_ = service->GetDescriptor()->full_name();
}

ServiceStub::~ServiceStub()
{
}

google::protobuf::Service* ServiceStub::GetService() const
{
    return service_.get();
}

const std::string& ServiceStub::FullName() const
{
    return name_;
}

void ServiceStub::SetUrlList(const std::string& serviceUrls)
{
    hardCodedUrls_.clear();

    auto urls = ananas::SplitString(serviceUrls, ';');
    hardCodedUrls_.reserve(urls.size());

    for (const auto& url : urls)
    {
        Endpoint ep(url);
        if (ep.IsValid())
            hardCodedUrls_.push_back(ep);
    }

    if (hardCodedUrls_.empty())
        ; // Warning
}

Future<ClientChannel* > ServiceStub::GetChannel()
{
    const Endpoint& ep = ChooseOne();
    return GetChannel(ep);
}

Future<ClientChannel* > ServiceStub::GetChannel(const Endpoint& ep)
{
    auto channels = GetChannelMap();

    auto it = channels->find(ep);
    if (it != channels->end())
        return MakeReadyFuture(it->second);
        
    return this->_Connect(ep);
}

Future<ClientChannel* > ServiceStub::_Connect(const Endpoint& ep)
{
    // TODO multi thread
    ChannelPromise promise;
    auto fut = promise.GetFuture();

    bool needConnect = false;
    {
        // Lock pendingConns_;
        auto it = pendingConns_.find(ep.addr);
        if (it == pendingConns_.end())
            needConnect = true;

        pendingConns_[ep.addr].emplace_back(std::move(promise)); 
    }
                    
    if (needConnect)
    {
        // TODO check UDP or TCP, now treat it as TCP
        Application::Instance().Connect(SocketAddr(ep.addr),
                                   std::bind(&ServiceStub::OnNewConnection, this, std::placeholders::_1),
                                   std::bind(&ServiceStub::OnConnFail, this, std::placeholders::_1, std::placeholders::_2),
                                   DurationMs(3000));
    }

    return fut;
}

void ServiceStub::OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer)
{ 
    auto req = pendingConns_.find(peer.ToString()); 
    if (req != pendingConns_.end()) 
    { 
        for (auto& prom : req->second) 
            prom.SetException(std::make_exception_ptr(std::runtime_error("Failed connect to " + peer.ToString()))); 
                 
        pendingConns_.erase(req); 
    }
}


void ServiceStub::SetOnCreateChannel(std::function<void (ClientChannel* )> cb)
{
    onCreateChannel_ = std::move(cb);
}

void ServiceStub::OnNewConnection(ananas::Connection* conn)
{
    auto channel = std::make_shared<ClientChannel>(conn, this);
    conn->SetUserData(channel);

    Endpoint ep;
    ep.proto = Endpoint::TCP;
    ep.addr = conn->Peer();
    bool succ = channels_->insert({ep, channel.get()}).second;
    assert (succ);

    if (onCreateChannel_)
        onCreateChannel_(channel.get());
                    
    conn->SetOnConnect(std::bind(&ServiceStub::_OnConnect, this, std::placeholders::_1));
    conn->SetOnDisconnect(std::bind(&ServiceStub::_OnDisconnect, this, std::placeholders::_1));
    conn->SetOnMessage(std::bind(&ServiceStub::OnMessage,
                                 this,
                                 std::placeholders::_1,
                                 std::placeholders::_2,
                                 std::placeholders::_3));
    conn->SetMinPacketSize(kPbHeaderLen); // 
}

void ServiceStub::OnRegister()
{
}

void ServiceStub::_OnConnect(ananas::Connection* conn)
{
    auto req = pendingConns_.find(conn->Peer());
    assert (req != pendingConns_.end());

    for (auto& prom : req->second)
        prom.SetValue(conn->GetUserData<ClientChannel>().get());

    pendingConns_.erase(req);
}

void ServiceStub::_OnDisconnect(ananas::Connection* conn)
{
    Endpoint ep;
    ep.proto = Endpoint::TCP;
    ep.addr = conn->Peer();
    channels_->erase(ep);
}

const Endpoint& ServiceStub::ChooseOne() const
{
    assert (!hardCodedUrls_.empty());

    // TODO it is very inefficient on MacOS
    std::random_device rd;
    int lucky = rd() % hardCodedUrls_.size();

    return hardCodedUrls_[lucky];
}

ServiceStub::ChannelMapPtr ServiceStub::GetChannelMap() 
{
    std::unique_lock<std::mutex> guard(channelMutex_);
    return channels_;
}


size_t ServiceStub::OnMessage(ananas::Connection* conn, const char* data, size_t len)
{
    const char* const start = data;
    size_t offset = 0;

    auto channel = conn->GetUserData<ClientChannel>();
    //while (1)
    {
        try {
            auto msg = channel->OnData(data, len - offset);
            if (msg)
            {
                channel->OnMessage(std::move(msg));
                offset += (data - start);
            }
        }
        catch (const std::exception& e) {
            printf("Some exception OnData %s\n", e.what());
            conn->ActiveClose();
            return 0;
        }
    }

    return data - start;
}

    
ClientChannel::ClientChannel(ananas::Connection* conn,
                             ananas::rpc::ServiceStub* service) :
    conn_(conn),
    service_(service)
{
}

ClientChannel::~ClientChannel()
{
}


ananas::Connection* ClientChannel::Connection() const
{
    return conn_;
}

int ClientChannel::GenId()
{
    return ++ reqIdGen_;
}

ananas::Buffer ClientChannel::PbToBytesEncoder(const std::string& method, const google::protobuf::Message& request)
{
    RpcMessage rpcMsg;
    encoder_.m2fEncoder_(&request, rpcMsg);

    // post process frame
    Request* req = rpcMsg.mutable_request();
    if (!HasField(*req, "id")) req->set_id(this->GenId());
    if (!HasField(*req, "service_name")) req->set_service_name(service_->FullName());
    if (!HasField(*req, "method_name")) req->set_method_name(method);

    if (encoder_.f2bEncoder_)
        return encoder_.f2bEncoder_(rpcMsg);
    else
    {
        ananas::Buffer bytes;
        auto data = req->serialized_request();
        bytes.PushData(data.data(), data.size());
        return bytes;
    }
}

std::shared_ptr<google::protobuf::Message> ClientChannel::OnData(const char*& data, size_t len)
{
    return decoder_.b2mDecoder_(data, len);
}

bool ClientChannel::OnMessage(std::shared_ptr<google::protobuf::Message> msg)
{
    // msg must have id
    RpcMessage* frame = dynamic_cast<RpcMessage*>(msg.get());
    if (frame)
    {
        const int id = frame->response().id();
        auto it = pendingCalls_.find(id);
        if (it != pendingCalls_.end())
            it->second.promise.SetValue(std::move(msg));
        else
            return false;// what fucking happened?

        pendingCalls_.erase(it);
        return true;
    }
    else
    {
        printf("RpcMessage bad_cast!\n");
        // default: FIFO, pop the first promise, TO use std::map
        auto it = pendingCalls_.begin();
        it->second.promise.SetValue(std::move(msg));
        pendingCalls_.erase(it);
    }

    return false;
}

void ClientChannel::SetEncoder(Encoder enc)
{
    encoder_ = std::move(enc);
}

void ClientChannel::SetDecoder(Decoder dec)
{
    decoder_ = std::move(dec);
}
    

thread_local int ClientChannel::reqIdGen_ {0};

} // end namespace rpc

} // end namespace ananas

