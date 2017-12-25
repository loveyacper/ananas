
#include <signal.h>
#include <cstring>
#include <cstdio>

#include "Application.h"
#include "AnanasLogo.h"
#include "EventLoopGroup.h"
#include "log/Logger.h"

static void SignalHandler(int num)
{
    ananas::Application::Instance().Exit();
}

static void InitSignal()
{
    struct sigaction sig;
    ::memset(&sig, 0, sizeof(sig));
   
    sig.sa_handler = SignalHandler;
    sigaction(SIGINT, &sig, NULL);
                                  
    // ignore sigpipe
    sig.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sig, NULL);

#ifdef ANANAS_LOGO
    // logo
    printf("%s\n", ananas::internal::logo);
#endif
}


namespace ananas
{

Application::~Application()
{
}

Application& Application::Instance()
{
    static Application app;
    return app;
}

void Application::_AddGroup(EventLoopGroup* group)
{
    assert (state_ == State::eS_None);
    groups_.push_back(group);
    printf("num of EventLoopGroup : %u\n", (unsigned)groups_.size());
}

void Application::Run()
{
    for (auto& group : groups_)
    {
        printf("Start EventLoopGroup ...\n");
        group->Start();
    }

    for (auto& group : groups_)
    {
        group->Wait();
        printf("Stop EventLoopGroup ...\n");
    }

    // thread safe exit
    LogManager::Instance().Stop();
}

void Application::Exit()
{
    if (state_ == State::eS_Stopped)
        return;

    state_ = State::eS_Stopped;
    for (auto g : groups_)
        g->Stop();
}

bool Application::IsExit() const
{
    return state_ == State::eS_Stopped;
}

Application::Application() :
    state_ {State::eS_None}
{
    InitSignal();
}

} // end namespace ananas

