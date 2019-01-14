#include <thread>
#include <cstdio>
#include "future/Future.h"
#include "net/EventLoop.h"
#include "net/Application.h"
#include "util/ThreadPool.h"

using namespace ananas;

template <typename Type>
Type ThreadFunc() {
    printf("Trying SetValue to 10 after 1 seconds\n");
    std::this_thread::sleep_for(std::chrono::seconds(1));

    return Type(10);
}

int main(int ac, char* av[]) {
    auto& app = Application::Instance();
    auto& loop = *app.BaseLoop();

    ananas::ThreadPool tpool;

    Future<int> ft(tpool.Execute(ThreadFunc<int>));

    ft.Then([](int v) {
        printf("!!!SUCC!\n");
        printf("Then got int value: %d\n", v);
    })
    .OnTimeout(std::chrono::seconds(1), []() {
        printf("!!!FAILED: future is timeout!\n");
    }, &loop);

    // exit after 2 seconds
    loop.ScheduleAfter(std::chrono::seconds(2), [&app]() {
        printf("BYE: now exiting!\n");
        app.Exit();
    });

    app.Run(ac, av);

    tpool.JoinAll();
    return 0;
}

