#include <thread>
#include <iostream>
#include "future/Future.h"
#include "net/EventLoop.h"

using namespace ananas;

template <typename Type>
void ThreadFunc(Promise<Type>& pm)
{
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "SetValue 10 after 1 second\n";
    Type v = 10;
    pm.SetValue(v);
}

int main()
{
    ananas::EventLoop loop;

    Promise<int> pm;
    std::thread t(ThreadFunc<int>, std::ref(pm));

    Future<int> ft(pm.GetFuture());
             
    ft.Then([](int v) {
            std::cerr << "1.Then got int value " << v << std::endl;
            })
            .OnTimeout(std::chrono::milliseconds(500), []() {
                    std::cerr << " Timeout..." << std::endl;
                },
                &loop
                );

    loop.ScheduleAfter(std::chrono::seconds(3), EventLoop::ExitAll);
    loop.Run();
    t.join();

    return 0;
}
