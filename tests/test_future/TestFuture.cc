#include <iostream>
#include "net/EventLoop.h"
#include "net/Application.h"
#include "util/ThreadPool.h"
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

int main(int ac, char* av[])
{
    std::cout << "main id " << std::this_thread::get_id() << std::endl;
    auto& app = Application::Instance();
    auto& loop = *app.BaseLoop();

    ananas::ThreadPool tpool;

    Future<int> ft(tpool.Execute(ThreadFunc<int>));

    ft.Then(&loop, [](int v) {
        std::cout << "1.Then got int value " << v
                  << " and return float 1.0f." << std::endl;
        return 1.0f;
    })
    .Then([](float f) {
        std::cout << "2.Then got float value " << f
                  << " and return 2." << std::endl;
        return 2;
    })
    .Then(&loop, [&tpool](int v) {
        std::cout << "3.Then got " << v << " and return another future\n";

        return tpool.Execute(ThreadFuncV);
    })
    .Then([]() {
        std::cout << "4. Then GOODBYE!\n";
        Application::Instance().Exit();
    });

    std::cout << "BEGIN LOOP" << std::endl;

    loop.ScheduleAfterWithRepeat<ananas::kForever>(std::chrono::seconds(1), []() {
            printf("every 1 second\n");
            });

    app.Run(ac, av);
    tpool.JoinAll();
            
    return 0;
}

