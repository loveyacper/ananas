#include <iostream>
#include "net/EventLoop.h"
#include "net/Application.h"
#include "util/ThreadPool.h"
#include "future/Future.h"

using namespace ananas;

template <typename Type>
Type ThreadException()
{
    throw std::runtime_error("Future Exception!");
    return Type(); // never here
}

void ThreadFuncV()
{
    std::cout << "SetValue Void\n";
}

int main(int ac, char* av[])
{
    auto& app = Application::Instance();
    auto& loop = *app.BaseLoop();

    ananas::ThreadPool tpool;

    Promise<int> pm;
    Future<int> ft(tpool.Execute(ThreadException<int>));

    ft.Then(&loop, [&loop](Try<int>&& v) {
        assert(loop.IsInSameLoop());
        try{
            int value = v;
            std::cout << "1.Then got int value " << value
                      << " and return float 1.0f." << std::endl;
        } catch (const std::exception& e) {
            std::cout << "1.Then got exception:" << e.what() << std::endl;
            return -1.0f;
        }

        return 1.0f;
    })
    .Then([](float f) {
        std::cout << "2.Then got float value " << f
                  << " and return 2." << std::endl;
        return 2;
    })
    .Then([&tpool](int v) {
        std::cout << "3.Then got " << v << " and return another future\n";

        auto future = tpool.Execute(ThreadFuncV);
        //std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return future;
    })
    .Then(&loop, [&app, &loop]() {
        assert(loop.IsInSameLoop());
        std::cout << "4. Then GOODBYE!\n";
        app.Exit();
    });

    std::cout << "BEGIN LOOP" << std::endl;

    loop.ScheduleAfterWithRepeat<ananas::kForever>(std::chrono::seconds(1), []() {
        std::cout << "every 1 second\n";
    });

    app.Run(ac, av);
            
    tpool.JoinAll();
    return 0;
}

