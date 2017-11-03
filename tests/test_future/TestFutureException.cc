#include <iostream>
#include "net/EventLoop.h"
#include "net/ThreadPool.h"
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

int main()
{
    static const auto mainId = std::this_thread::get_id();

    ananas::EventLoop loop;
    auto& tpool = ananas::ThreadPool::Instance();

    Promise<int> pm;
    Future<int> ft(tpool.Execute(ThreadException<int>));

    ft.Then(&loop, [](Try<int>&& v) {
        assert(mainId == std::this_thread::get_id());
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
                  << " and return nothing." << std::endl;
    })
    .Then([&tpool]() {
        std::cout << "3.Then got void and return another future\n";

        auto future = tpool.Execute(ThreadFuncV);
        //std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return future;
    })
    .Then(&loop, []() {
        assert(mainId == std::this_thread::get_id());
        std::cout << "4. Then GOODBYE!\n";
        ananas::EventLoop::ExitApplication();
    });

    std::cout << "BEGIN LOOP" << std::endl;

    loop.ScheduleAfter<ananas::kForever>(std::chrono::seconds(1), []() {
        std::cout << "every 1 second\n";
    });

    loop.Run();
            
    return 0;
}

