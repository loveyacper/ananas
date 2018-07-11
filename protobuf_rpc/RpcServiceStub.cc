#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include <memory>
#include <string>
#include <vector>

#include "RpcServiceStub.h"
#include "RpcServer.h"
#include "net/Socket.h"
#include "net/Application.h"
#include "util/Util.h"

#include "ananas_rpc.pb.h"

namespace ananas
{

namespace rpc
{

static const std::string idStr("id");
static const std::string svrStr("service_name");
static const std::string methodStr("method_name");

ServiceStub::ServiceStub(google::protobuf::Service* service)
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
    // called from main, only once
    assert (!hardCodedUrls_);
    hardCodedUrls_ = std::make_shared<std::vector<Endpoint>>();

    auto urls = ananas::SplitString(serviceUrls, ';');
    hardCodedUrls_->reserve(urls.size());

    for (const auto& url : urls)
    {
        Endpoint ep = EndpointFromString(url);
        if (!ep.ip().empty())
        {
            hardCodedUrls_->push_back(ep);
        }
    }

    if (hardCodedUrls_->empty())
        ANANAS_WRN << "No valid url : " << serviceUrls;
}

Future<ClientChannel* > ServiceStub::GetChannel()
{
    auto loop = EventLoop::GetCurrentEventLoop();
    if (!loop || loop == RPC_SERVER.BaseLoop())
        loop = RPC_SERVER.Next();

    if (loop->IsInSameLoop())
        return _GetEndpoints()
              .Then(std::bind(&ServiceStub::_SelectChannel, this, loop, std::placeholders::_1));
    else
        return _GetEndpoints()
              .Then(loop, std::bind(&ServiceStub::_SelectChannel, this, loop, std::placeholders::_1));
}

Future<ClientChannel* > ServiceStub::GetChannel(const Endpoint& ep)
{
    if (IsValidEndpoint(ep))
    {
        auto loop = EventLoop::GetCurrentEventLoop();
        if (!loop || loop == RPC_SERVER.BaseLoop())
            loop = RPC_SERVER.Next();

        if (loop->IsInSameLoop())
            return _MakeChannel(loop, ep);
        else
            return loop->Execute(std::bind(&ServiceStub::_MakeChannel, this, loop, ep)).Unwrap();
    }
    else
    {
        return GetChannel();
    }
}

Future<ClientChannel* > ServiceStub::_SelectChannel(EventLoop* loop, Try<std::shared_ptr<std::vector<Endpoint>>>&& eps)
{
    assert (loop->IsInSameLoop());
    try {
        return _MakeChannel(loop, _SelectEndpoint(eps));
    }
    catch (...) {
        return MakeExceptionFuture<ClientChannel*>(std::current_exception());
    }
} 

Future<ClientChannel* > ServiceStub::_MakeChannel(EventLoop* loop, const Endpoint& ep)
{
    assert (loop->IsInSameLoop());

    if (!IsValidEndpoint(ep))
        return MakeExceptionFuture<ClientChannel*>(std::runtime_error("No available endpoint"));

    const auto& channels = channels_[loop->Id()];

    auto it = channels.find(ep);
    if (it != channels.end())
        return MakeReadyFuture(it->second.get());
        
    return this->_Connect(loop, ep);
}

Future<ClientChannel* > ServiceStub::_Connect(EventLoop* loop, const Endpoint& ep)
{
    assert (loop->IsInSameLoop());

    ChannelPromise promise;
    auto fut = promise.GetFuture();

    const SocketAddr dst(EndpointToSocketAddr(ep));
    auto& pendingConns = pendingConns_[loop->Id()];

    auto it = pendingConns.find(dst);
    const bool needConnect = (it == pendingConns.end());

    pendingConns[dst].emplace_back(std::move(promise)); 
                    
    if (needConnect)
    {
        ANANAS_INF << "Connect to " << dst.ToString() << " in loop " << loop->Id();
        // TODO check UDP or TCP, now treat it as TCP
        Application::Instance().Connect(dst,
                                        std::bind(&ServiceStub::_OnNewConnection,
                                                  this,
                                                  std::placeholders::_1),
                                        std::bind(&ServiceStub::_OnConnFail,
                                                  this,
                                                  std::placeholders::_1,
                                                  std::placeholders::_2),
                                        DurationMs(3000),
                                        loop);
    }

    return fut;
}

void ServiceStub::_OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer)
{ 
    assert (loop->IsInSameLoop());
    const int id = loop->Id(); 
    auto& pendingConns = pendingConns_[id];
    auto req = pendingConns.find(peer); 
    if (req != pendingConns.end()) 
    { 
        for (auto& prom : req->second) 
            prom.SetException(std::make_exception_ptr(std::runtime_error("Failed connect to " + peer.ToString()))); 
                 
        pendingConns.erase(req); 
    }
}

void ServiceStub::SetOnCreateChannel(std::function<void (ClientChannel* )> cb)
{
    onCreateChannel_ = std::move(cb);
}

void ServiceStub::_OnNewConnection(ananas::Connection* conn)
{
    assert (conn->GetLoop()->IsInSameLoop());

    auto _ = std::static_pointer_cast<ananas::Connection>(conn->shared_from_this());
    auto channel = std::make_shared<ClientChannel>(_, this);
    conn->SetUserData(channel);

    {
        Endpoint ep;
        ep.set_proto(TCP);
        ep.set_ip(conn->Peer().GetIP());
        ep.set_port(conn->Peer().GetPort());
        auto& channelMap = channels_[conn->GetLoop()->Id()];
        bool succ = channelMap.insert({ep, channel}).second;
        assert (succ);
    }

    if (onCreateChannel_)
        onCreateChannel_(channel.get());
                    
    conn->SetOnConnect(std::bind(&ServiceStub::_OnConnect, this, std::placeholders::_1));
    conn->SetOnDisconnect(std::bind(&ServiceStub::_OnDisconnect, this, std::placeholders::_1));
    conn->SetOnMessage(&ServiceStub::_OnMessage);
    conn->SetMinPacketSize(kPbHeaderLen);
}

void ServiceStub::OnRegister()
{
    channels_.resize(Application::Instance().NumOfWorker());
    pendingConns_.resize(Application::Instance().NumOfWorker());
}

void ServiceStub::_OnConnect(ananas::Connection* conn)
{
    // It's called in conn's EventLoop, see `ananas::Connector::_OnSuccess`
    assert (conn->GetLoop()->IsInSameLoop());

    auto& pendingConns = pendingConns_[conn->GetLoop()->Id()];
    auto req = pendingConns.find(conn->Peer());
    assert (req != pendingConns.end());

    std::vector<ChannelPromise> promises(std::move(req->second));
    pendingConns.erase(req);

    // channelFuture will be fulfilled
    for (auto& prom : promises)
        prom.SetValue(conn->GetUserData<ClientChannel>().get());
}

void ServiceStub::_OnDisconnect(ananas::Connection* conn)
{
    Endpoint ep;
    ep.set_proto(TCP);
    ep.set_ip(conn->Peer().GetIP());
    ep.set_port(conn->Peer().GetPort());
        
    auto& channelMap = channels_[conn->GetLoop()->Id()];
    auto it = channelMap.find(ep);
    assert (it != channelMap.end());
    it->second->OnDestroy();
    channelMap.erase(it);
}

Future<std::shared_ptr<std::vector<Endpoint>>> ServiceStub::_GetEndpoints()
{
    if (hardCodedUrls_ && !hardCodedUrls_->empty())
        return MakeReadyFuture(hardCodedUrls_);

    // rpc::Call() can be called everywhere, so protect these code
    std::unique_lock<std::mutex> guard(endpointsMutex_);
    if (endpoints_ && !endpoints_->empty())
    {
        return MakeReadyFuture(endpoints_);
    }
    else
    {
        // No endpoints, we need visit NameServer
        Promise<std::shared_ptr<std::vector<Endpoint>>> promise;
        auto future = promise.GetFuture();

        bool needRefresh = pendingEndpoints_.empty(); // TODO:Timeout is checked by timer
        pendingEndpoints_.emplace_back(std::move(promise));
        guard.unlock();

        if (needRefresh)
        {
            // call NameServer to GetEndpoints
            ananas::rpc::ServiceName name;
            name.set_name(this->FullName());
            auto scheduler = EventLoop::GetCurrentEventLoop();
            assert (scheduler);
            rpc::Call<EndpointList>("ananas.rpc.NameService", "GetEndpoints", name)
                .Then(scheduler, std::bind(&ServiceStub::_OnNewEndpointList, this, std::placeholders::_1))
                .OnTimeout(std::chrono::seconds(3), [this]()
                {
                    std::unique_lock<std::mutex> guard(endpointsMutex_);
                    for (auto& promise : pendingEndpoints_)
                    {
                        promise.SetException(std::make_exception_ptr(std::runtime_error("GetEndpoints timeout")));
                    }
                    pendingEndpoints_.clear();
                    pendingEndpoints_.shrink_to_fit();
                },
                scheduler);
        }

        return future;
    }
}

void ServiceStub::_OnNewEndpointList(Try<EndpointList>&& endpoints)
{
    try {
        EndpointList eps = std::move(endpoints);
        ANANAS_DBG << "From nameserver, GetEndpoints got : " << eps.DebugString();
                          
        auto newEndpoints = std::make_shared<std::vector<Endpoint>>();
        for  (int i = 0; i < eps.endpoints_size(); ++ i)
        {
            const auto& e = eps.endpoints(i);
            newEndpoints->push_back(e);
        }

        {
            std::unique_lock<std::mutex> guard(endpointsMutex_);
            this->endpoints_ = newEndpoints;
        }
       
        for (auto& promise : pendingEndpoints_)
        {
            promise.SetValue(newEndpoints);
        }
        pendingEndpoints_.clear();
        pendingEndpoints_.shrink_to_fit();
    }
    catch (const std::exception& e) {
        // Do not clear cache if exception from name server
        ANANAS_ERR << "GetEndpoints exception:" << e.what();
        std::unique_lock<std::mutex> guard(endpointsMutex_);
        auto endpoints = this->endpoints_;
        for (auto& promise : pendingEndpoints_)
        {
            promise.SetValue(endpoints);
        }
        pendingEndpoints_.clear();
        pendingEndpoints_.shrink_to_fit();
    }
}

const Endpoint& ServiceStub::_SelectEndpoint(std::shared_ptr<std::vector<Endpoint>> eps)
{
    if (!eps || eps->empty())
        return Endpoint::default_instance();

    // TEMPORARY CODE TODO load balance
    static int current = 0;
    const int lucky = current++ % static_cast<int>(eps->size());

    return (*eps)[lucky];
}


size_t ServiceStub::_OnMessage(ananas::Connection* conn, const char* data, size_t len)
{
    const char* const start = data;
    size_t offset = 0;

    auto channel = conn->GetUserData<ClientChannel>();
    // TODO process message like redis
    try {
        auto msg = channel->OnData(data, len - offset);
        if (msg)
        {
            channel->OnMessage(std::move(msg));
            offset += (data - start);
        }
    }
    catch (const std::exception& e) {
        // Often because evil message
        ANANAS_ERR << "Some exception OnData: " << e.what();
        conn->ActiveClose();
        return 0;
    }

    return data - start;
}

    
ClientChannel::ClientChannel(std::shared_ptr<Connection> conn,
                             ananas::rpc::ServiceStub* service) :
    conn_(conn),
    service_(service),
    encoder_(PbToFrameRequestEncoder)
{
    pendingTimeoutId_ = conn->GetLoop()->ScheduleAfterWithRepeat<ananas::kForever>(
                                             std::chrono::seconds(1),
                                             std::bind(&ClientChannel::_CheckPendingTimeout, this)
                                         );
}

ClientChannel::~ClientChannel()
{
}

void ClientChannel::SetContext(std::shared_ptr<void> ctx)
{
    ctx_ = std::move(ctx);
}

int ClientChannel::GenId()
{
    return ++ reqIdGen_;
}

ananas::Buffer ClientChannel::_MessageToBytesEncoder(std::string&& method,
                                                     const google::protobuf::Message& request)
{
    RpcMessage rpcMsg;
    encoder_.m2fEncoder_(&request, rpcMsg);

    // post process frame
    Request* req = rpcMsg.mutable_request();
    if (!HasField(*req, svrStr)) req->set_service_name(service_->FullName());
    if (!HasField(*req, methodStr)) req->set_method_name(std::move(method));
    if (!HasField(*req, idStr))
        req->set_id(this->GenId());
    else
        reqIdGen_ = req->id(); // other protocol may has its own req-id, such as http2

    if (encoder_.f2bEncoder_)
    {
        // eg. add 4 bytes to indicate the frame length
        return encoder_.f2bEncoder_(rpcMsg);
    }
    else
    {
        // if no f2bEncoder_, then send the serialized_request directly
        // eg. The text protocol
        ananas::Buffer bytes;
        const auto& data = req->serialized_request();
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
    RpcMessage* frame = dynamic_cast<RpcMessage*>(msg.get());
    if (frame)
    {
        assert (HasField(frame->response(), idStr));

        const int id = frame->response().id();
        auto it = pendingCalls_.find(id);
        if (it != pendingCalls_.end())
        {
            it->second.promise.SetValue(std::move(msg));
        }
        else
        {
            ANANAS_ERR << "ClientChannel::OnMessage can not find " << id << ", maybe TIMEOUT already.";
            return false;
        }

        pendingCalls_.erase(it);
        return true;
    }
    else
    {
        ANANAS_INF << "Don't panic: RpcMessage bad_cast, may be text message";
        // default: FIFO, pop the first promise
        // but what if int overflow?
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

void ClientChannel::OnDestroy()
{
    if (pendingTimeoutId_)
    {
        auto c = conn_.lock();
        if (c)
            c->GetLoop()->Cancel(pendingTimeoutId_);

        pendingTimeoutId_.reset();
    }
}
    
void ClientChannel::_CheckPendingTimeout()
{
    // TODO set max timeout, default 15s
    ananas::Time now;
    for (auto it(pendingCalls_.begin()); it != pendingCalls_.end(); )
    {
        if (now < it->second.timestamp + 15 * 1000)
            return;
        
        if (it->second.promise.IsReady())
            ANANAS_DBG << "Checkout for erase pending call id :" << it->first;
        else
            ANANAS_ERR << "TIMEOUT: Checkout for pending call id :" << it->first;

        it = pendingCalls_.erase(it);
    }
}

thread_local int ClientChannel::reqIdGen_ {0};

} // end namespace rpc

} // end namespace ananas

