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

void ThreadLoop(ananas::EventLoop* loop)
{
    loop->Run();
}


int main()
{
    ananas::EventLoop loop;
    ananas::EventLoop loop2;

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
    .Then(&loop2, [](float f) {
        std::cerr << "2.Then got float value " << f
                  << " and return nothing." << std::endl;
    })
    .Then(&loop, [&tpool, &pmv]() {
        std::cerr << "3.Then got void and return another future\n";

        tpool.Execute(ThreadFuncV, std::ref(pmv));
        return pmv.GetFuture();
    })
    .Then(&loop2, []() {
        std::cerr << "4. Then GOODBYE!\n";
        ananas::EventLoop::ExitApplication();
    });

            
    printf("BEGIN LOOP\n");

    tpool.Execute(ThreadLoop, &loop2);

    loop.ScheduleAfter<ananas::kForever>(std::chrono::seconds(1), []() {
            printf("every 1 second\n");
            });
    loop.Run();
            
    return 0;
}

