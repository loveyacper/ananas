#include <iostream>
#include "net/EventLoop.h"
#include "net/ThreadPool.h"
#include "future/Future.h"

using namespace ananas;

template <typename Type>
void ThreadFunc(Promise<Type>& pm)
{
    std::cout << "SetValue 10\n";
    Type v = 10;
    pm.SetValue(v);
}

void ThreadFuncV(Promise<void>& pm)
{
    std::cout << "SetValue Void\n";
    pm.SetValue();
}

int main()
{
    ananas::EventLoop loop;

    auto& tpool = ananas::ThreadPool::Instance();

    Promise<int> pm;
    tpool.Execute(ThreadFunc<int>, std::ref(pm));

    Future<int> ft(pm.GetFuture());
    Promise<void> pmv;

    ft.Then(&loop, [](int v) {
        std::cerr << "1.Then got int value " << v
                  << " and return float 1.0f." << std::endl;
        return 1.0f;
    })
    .Then([](float f) {
        std::cerr << "2.Then got float value " << f
                  << " and return nothing." << std::endl;
    })
    .Then(&loop, [&tpool, &pmv]() {
        std::cerr << "3.Then got void and return another future\n";

        tpool.Execute(ThreadFuncV, std::ref(pmv));
        return pmv.GetFuture();
    })
    .Then([]() {
        std::cerr << "4. Then GOODBYE!\n";
        ananas::EventLoop::ExitApplication();
    });

    std::cerr << "BEGIN LOOP" << std::endl;

    loop.ScheduleAfter<ananas::kForever>(std::chrono::seconds(1), []() {
            printf("every 1 second\n");
            });
    loop.Run();
            
    return 0;
}

