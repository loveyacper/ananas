#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "RpcServer.h"
#include "RpcService.h"
#include "RpcServiceStub.h"
#include "ananas/net/Application.h"
#include "ananas/net/AnanasDebug.h"

#include "RedisClientContext.h"

namespace ananas
{

namespace rpc
{
    
Server* Server::s_rpcServer = nullptr;

Server::Server() :
    app_(ananas::Application::Instance())
{
    assert (!s_rpcServer);
    s_rpcServer = this;
}

bool Server::AddService(ananas::rpc::Service* service)
{
    auto googleService = service->GetService();
    const auto& name = googleService->GetDescriptor()->full_name();
    ANANAS_INF << "AddService " << googleService->GetDescriptor()->name().data();
    std::unique_ptr<Service> svr(service);

    if (services_.insert(std::make_pair(StringView(name), std::move(svr))).second)
        service->OnRegister();
    else
        return false;

    return true;
}

bool Server::AddService(std::unique_ptr<Service>&& service)
{
    auto srv = service.get();
    auto googleService = service->GetService();
    const auto name = googleService->GetDescriptor()->full_name();

    if (services_.insert(std::make_pair(StringView(name), std::move(service))).second)
        srv->OnRegister();
    else
        return false;

    return true;
}

bool Server::AddServiceStub(ananas::rpc::ServiceStub* service)
{
    auto googleService = service->GetService();
    const auto& name = googleService->GetDescriptor()->full_name();
    ANANAS_INF << "AddServiceStub " << googleService->GetDescriptor()->name().data();
    std::unique_ptr<ServiceStub> svr(service);

    if (stubs_.insert(std::make_pair(StringView(name), std::move(svr))).second)
        service->OnRegister();
    else
        return false;

    return true;
}

bool Server::AddServiceStub(std::unique_ptr<ServiceStub>&& service)
{
    auto srv = service.get();
    auto googleService = service->GetService();
    const auto name = googleService->GetDescriptor()->full_name();

    if (stubs_.insert(std::make_pair(StringView(name), std::move(service))).second)
        srv->OnRegister();
    else
        return false;

    return true;
}

ananas::rpc::ServiceStub* Server::GetServiceStub(const ananas::StringView& name) const
{
    auto it = stubs_.find(name);
    return it == stubs_.end() ? nullptr : it->second.get();
}

void Server::SetNumOfWorker(size_t n)
{
    if (services_.empty())
        app_.SetNumOfWorker(n);
    else
        assert (!!!"Don't change worker number after service added");
}

void Server::Start(int ac, char* av[])
{
    for (const auto& kv : services_)
    {
        if (kv.second->Start())
        {
            ANANAS_INF << "start succ service " << kv.first.Data();
        }
        else
        {
            ANANAS_ERR << "start failed service " << kv.first.Data();
            return;
        }
    }

    if (nameServiceStub_)
    {
        ANANAS_DBG << "Use nameservice " << nameServiceStub_->FullName();
    }

    if (services_.empty())
    {
        ANANAS_WRN << "Warning: No available service";
    }
    else
    {
        BaseLoop()->ScheduleAfterWithRepeat<ananas::kForever>(std::chrono::seconds(3),
                [this]() {
                    if (keepaliveInfo_.size() == 0) {
                        for (const auto& kv : services_) {
                            KeepaliveInfo info;
                            info.set_servicename(kv.first.ToString());
                            info.mutable_endpoint()->CopyFrom(kv.second->GetEndpoint());
                            keepaliveInfo_.push_back(info);
                        }
                    }

                    ANANAS_DBG << "Call Keepalive";
                    for (const auto& e : keepaliveInfo_)
                         Call<Status>("ananas.rpc.NameService", "Keepalive", e);
                }
            );
    }

    app_.Run(ac, av);
}

void Server::Shutdown()
{
    app_.Exit();
}
    
Server& Server::Instance()
{
    return *Server::s_rpcServer;
}

EventLoop* Server::BaseLoop()
{
    return app_.BaseLoop();
}

EventLoop* Server::Next()
{
    return app_.Next();
}

void Server::SetOnInit(std::function<bool (int, char*[])> init)
{
    app_.SetOnInit(std::move(init));
}

void Server::SetOnExit(std::function<void ()> onexit)
{
    app_.SetOnExit(std::move(onexit));
}

void Server::SetNameServer(const std::string& url)
{
    assert (!nameServiceStub_);

    nameServiceStub_ = new ServiceStub(new NameService_Stub(nullptr));
    nameServiceStub_->SetUrlList(url);
    if (onCreateNameServiceChannel_)
        nameServiceStub_->SetOnCreateChannel(onCreateNameServiceChannel_);
    else
        nameServiceStub_->SetOnCreateChannel(OnCreateRedisChannel);

    ANANAS_DBG << "SetNameServer " << nameServiceStub_->FullName();
    
    this->AddServiceStub(nameServiceStub_);
}

void Server::SetOnCreateNameServerChannel(std::function<void (ClientChannel*)> occ)
{
    onCreateNameServiceChannel_ = std::move(occ);
}

} // end namespace rpc

} // end namespace ananas

