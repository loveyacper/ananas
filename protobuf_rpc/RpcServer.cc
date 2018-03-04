#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "RpcServer.h"
#include "RpcService.h"
#include "RpcServiceStub.h"
#include "net/Application.h"

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
    printf("AddService %s\n", googleService->GetDescriptor()->name().data());
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
    printf("AddServiceStub %s\n", googleService->GetDescriptor()->name().data());
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
            printf("start succ service %s\n", kv.first.data());
        }
        else
        {
            printf("start failed service %s\n", kv.first.data());
            return;
        }
    }

    if (services_.empty())
        printf("Warning: No available service\n");

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

} // end namespace rpc

} // end namespace ananas

