#include <thread>
#include <iostream>
#include "net/EventLoop.h"
#include "net/ThreadPool.h"
#include "future/Future.h"

using namespace ananas;

template <typename Type>
Type ThreadFunc()
{
    std::cout << "SetValue 10\n";
    return Type(10);
}

void ThreadFuncV()
{
    std::cout << "SetValue void\n";
}

int main()
{
    ananas::EventLoop loop;
    auto& tpool = ananas::ThreadPool::Instance();

    auto f1 = tpool.Execute(ThreadFunc<int>);
    auto f2 = tpool.Execute(ThreadFuncV);

    auto fall = WhenAll(f1, f2);
    fall.Then([](const std::tuple<Try<int>, Try<void>>& results) {
            std::cerr << "Then collet all!\n";
            std::cerr << std::get<0>(results) << std::endl;
         })
         .OnTimeout(std::chrono::milliseconds(500), []() {
             std::cout << "!!!FAILED: futureall is timeout!\n";
         }, &loop);

    loop.ScheduleAfter<1>(std::chrono::seconds(3), []() {
        std::cerr << "GOODBYE!\n";
        ananas::EventLoop::ExitApplication();
    });

    loop.Run();

    return 0;
}

