#include <iostream>
#include "net/EventLoop.h"
#include "net/ThreadPool.h"
#include "future/Future.h"

using namespace ananas;

template <typename Type>
Type ThreadFunc()
{
    std::cout << "SetValue 10\n";
    Type v = 10;
    return v;
}

void ThreadFuncV()
{
    std::cout << "SetValue Void\n";
}

int main()
{
    ananas::EventLoop loop;
    auto& tpool = ananas::ThreadPool::Instance();

    Future<int> ft(tpool.Execute(ThreadFunc<int>));

    ft.Then(&loop, [](int v) {
        std::cout << "1.Then got int value " << v
                  << " and return float 1.0f." << std::endl;
        return 1.0f;
    })
    .Then([](float f) {
        std::cout << "2.Then got float value " << f
                  << " and return nothing." << std::endl;
    })
    .Then(&loop, [&tpool]() {
        std::cout << "3.Then got void and return another future\n";

        return tpool.Execute(ThreadFuncV);
    })
    .Then([]() {
        std::cout << "4. Then GOODBYE!\n";
        ananas::EventLoop::ExitApplication();
    });

    std::cout << "BEGIN LOOP" << std::endl;

    loop.ScheduleAfter<ananas::kForever>(std::chrono::seconds(1), []() {
            printf("every 1 second\n");
            });

    loop.Run();
            
    return 0;
}

