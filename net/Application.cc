
#include <signal.h>
#include <cstring>
#include <cstdio>

#include <memory>
#include <mutex>
#include <condition_variable>

#include "util/Util.h"
#include "Application.h"
#include "AnanasLogo.h"
#include "Socket.h"
#include "AnanasDebug.h"

static void SignalHandler(int num) {
    ananas::Application::Instance().Exit();
}

static void InitSignal() {
    struct sigaction sig;
    ::memset(&sig, 0, sizeof(sig));

    sig.sa_handler = SignalHandler;
    sigaction(SIGINT, &sig, NULL);

    // ignore sigpipe
    sig.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sig, NULL);
}


namespace ananas {

Application::~Application() {
}

Application& Application::Instance() {
    static Application app;
    return app;
}

void Application::SetNumOfWorker(size_t num) {
    assert (state_ == State::eS_None);
    assert (num <= 512);

    numLoop_ = num;
}

size_t Application::NumOfWorker() const {
    // plus one : the baseLoop
    return 1 + numLoop_;
}

void Application::Run(int ac, char* av[]) {
    ANANAS_DEFER {
        if (onExit_)
            onExit_();

        // thread safe exit
        LogManager::Instance().Stop();
    };

    if (state_ != State::eS_None)
        return;

    if (onInit_) {
        if (!onInit_(ac, av)) {
            printf("onInit FAILED, exit!\n");
            return;
        }
    }

    // start loops in thread pool
    _StartWorkers();
    BaseLoop()->Run();

    printf("Stopped BaseEventLoop...\n");

    pool_.JoinAll();
    loops_.clear();
    numLoop_ = 0;
    printf("Stopped WorkerEventLoops...\n");
}

void Application::Exit() {
    if (state_ == State::eS_Stopped)
        return;

    state_ = State::eS_Stopped;
}

bool Application::IsExit() const {
    return state_ == State::eS_Stopped;
}

EventLoop* Application::BaseLoop() {
    return &base_;
}

void Application::SetOnInit(std::function<bool (int, char*[])> init) {
    onInit_ = std::move(init);
}

void Application::SetOnExit(std::function<void ()> onexit) {
    onExit_ = std::move(onexit);
}

void Application::Listen(const SocketAddr& listenAddr,
                         NewTcpConnCallback cb,
                         BindCallback bfcb) {
    auto loop = BaseLoop();
    loop->Execute([loop, listenAddr, cb, bfcb]() {
        if (!loop->Listen(listenAddr, std::move(cb)))
            bfcb(false, listenAddr);
        else
            bfcb(true, listenAddr);
    });
}

void Application::Listen(const char* ip,
                         uint16_t hostPort,
                         NewTcpConnCallback cb,
                         BindCallback bfcb) {
    SocketAddr addr(ip, hostPort);
    Listen(addr, std::move(cb), std::move(bfcb));
}

void Application::ListenUDP(const SocketAddr& addr,
                            UDPMessageCallback mcb,
                            UDPCreateCallback ccb,
                            BindCallback bfcb) {
    auto loop = BaseLoop();
    loop->Execute([loop, addr, mcb, ccb, bfcb]() {
        if (!loop->ListenUDP(addr, std::move(mcb), std::move(ccb)))
            bfcb(false, addr);
        else
            bfcb(true, addr);
    });
}

void Application::ListenUDP(const char* ip, uint16_t hostPort,
                            UDPMessageCallback mcb,
                            UDPCreateCallback ccb,
                            BindCallback bfcb) {
    SocketAddr addr(ip, hostPort);
    ListenUDP(addr, std::move(mcb), std::move(ccb), std::move(bfcb));
}

void Application::CreateClientUDP(UDPMessageCallback mcb,
                                  UDPCreateCallback ccb) {
    auto loop = BaseLoop();
    loop->Execute([loop, mcb, ccb]() {
        loop->CreateClientUDP(std::move(mcb), std::move(ccb));
    });
}

void Application::Connect(const SocketAddr& dst,
                          NewTcpConnCallback nccb,
                          TcpConnFailCallback cfcb,
                          DurationMs timeout,
                          EventLoop* dstLoop) {
    auto loop = BaseLoop();
    loop->Execute([loop, dst, nccb, cfcb, timeout, dstLoop]() {
        loop->Connect(dst,
                      std::move(nccb),
                      std::move(cfcb),
                      timeout,
                      dstLoop);
    });
}

void Application::Connect(const char* ip,
                          uint16_t hostPort,
                          NewTcpConnCallback nccb,
                          TcpConnFailCallback cfcb,
                          DurationMs timeout,
                          EventLoop* dstLoop) {
    SocketAddr dst(ip, hostPort);
    Connect(dst, std::move(nccb), std::move(cfcb), timeout, dstLoop);
}


EventLoop* Application::Next() {
    if (state_ != State::eS_Started)
        return BaseLoop();

    if (loops_.empty())
        return BaseLoop();

    auto& loop = loops_[currentLoop_++ % loops_.size()];
    return loop.get();
}

void Application::_StartWorkers() {
    // only called by main thread
    assert (state_ == State::eS_None);

    std::mutex mutex;
    std::condition_variable cond;

    pool_.SetNumOfThreads(numLoop_);
    for (size_t i = 0; i < numLoop_; ++i) {
        pool_.Execute([this, &mutex, &cond]() {
            EventLoop* loop(new EventLoop);

            {
                std::unique_lock<std::mutex> guard(mutex);
                loops_.push_back(std::unique_ptr<EventLoop>(loop));
                if (loops_.size() == numLoop_)
                    cond.notify_one();
            }

            loop->Run();
        });
    }

    std::unique_lock<std::mutex> guard(mutex);
    cond.wait(guard, [this] () {
        return loops_.size() == numLoop_;
    });

    state_ = State::eS_Started;
}

Application::Application() :
    state_ {State::eS_None} {
    InitSignal();

    // logo
    fprintf(stdout, "%s", "\033[1;36;40m");
    printf("%s\n", ananas::internal::logo);
    fprintf(stdout, "%s", "\033[0m");
}

void Application::_DefaultBindCallback(bool succ, const SocketAddr& listenAddr) {
    if (succ) {
        ANANAS_INF << "Listen succ for " << listenAddr.ToString();
    } else {
        ANANAS_ERR << "Listen failed for " << listenAddr.ToString();
        Application::Instance().Exit();
    }
}

} // end namespace ananas

