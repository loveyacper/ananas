#include <iostream>
#include "net/EventLoop.h"
#include "net/ThreadPool.h"
#include "future/Future.h"

using namespace ananas;

template <typename Type>
void ThreadException(Promise<Type>& pm)
{
    try {
        throw std::runtime_error("Future Exception!");
    } catch (...) {
        pm.SetException(std::current_exception());
        return;
    }
}

void ThreadFuncV(Promise<void>& pm)
{
    std::cout << "SetValue Void\n";
    pm.SetValue();
}

int main()
{
    static const auto mainId = std::this_thread::get_id();

    ananas::EventLoop loop;

    auto& tpool = ananas::ThreadPool::Instance();

    Promise<int> pm;
    tpool.Execute(ThreadException<int>, std::ref(pm));

    Future<int> ft(pm.GetFuture());
    Promise<void> pmv;

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
    .Then([&tpool, &pmv]() {
        std::cout << "3.Then got void and return another future\n";

        tpool.Execute(ThreadFuncV, std::ref(pmv));
        //std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return pmv.GetFuture();
    })
    .Then(&loop, []() {
        assert(mainId == std::this_thread::get_id());
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

