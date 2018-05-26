#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "RpcServer.h"
#include "RpcService.h"
#include "RpcServiceStub.h"
#include "net/Application.h"
#include "net/AnanasDebug.h"

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

    if (services_.insert(std::make_pair(name, std::move(svr))).second)
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

    if (services_.insert(std::make_pair(name, std::move(service))).second)
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

    if (stubs_.insert(std::make_pair(name, std::move(svr))).second)
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

    if (stubs_.insert(std::make_pair(name, std::move(service))).second)
        srv->OnRegister();
    else
        return false;

    return true;
}

ananas::rpc::ServiceStub* Server::GetServiceStub(const std::string& name) const
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

void Server::Start()
{
    for (const auto& kv : services_)
    {
        if (kv.second->Start())
        {
            ANANAS_INF << "start succ service " << kv.first.data();
        }
        else
        {
            ANANAS_ERR << "start failed service " << kv.first.data();
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
        //if (services_.size() > 1 || services_.begin()->first != "ananas.rpc.NameService")
        //TODO 
        BaseLoop()->ScheduleAfterWithRepeat<ananas::kForever>(std::chrono::seconds(3),
                [this]() {
                    if (keepaliveInfo_.info_size() == 0)
                    {
                        for (const auto& kv : services_)
                        {
                            Heartbeat& info = *keepaliveInfo_.add_info();
                            info.set_servicename(kv.first);
                            info.mutable_endpoint()->CopyFrom(kv.second->GetEndpoint());
                        }
                    }

                    ANANAS_DBG << "Call Keepalive";
                    Call<Status>("ananas.rpc.NameService", "Keepalive", keepaliveInfo_);
#if 0
                    .Then([](Status s) {
                            printf("Keepalive succ\n");
                            });
#endif
                }
            );
    }

    app_.Run();
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
    
void Server::SetNameServer(const std::string& url)
{
    assert (!nameServiceStub_);

    nameServiceStub_ = new ServiceStub(new NameService_Stub(nullptr));
    nameServiceStub_->SetUrlList(url);

    ANANAS_DBG << "SetNameServer " << nameServiceStub_->FullName();
    
    this->AddServiceStub(nameServiceStub_);
}

} // end namespace rpc

} // end namespace ananas

