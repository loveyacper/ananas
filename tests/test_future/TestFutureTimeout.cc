#include <thread>
#include <cstdio>
#include "future/Future.h"
#include "net/EventLoop.h"

using namespace ananas;

template <typename Type>
void ThreadFunc(Promise<Type>& pm)
{
    printf("Trying SetValue to 10 after 1 seconds\n");
    std::this_thread::sleep_for(std::chrono::seconds(1));

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
                printf("!!!SUCC\n");
                printf("1.Then got int value: %d\n", v);
                return std::string("dummy string");
          })
        .Then([] (std::string s) {
                printf("2.Then got string value: %s\n", s.data());
          })
        .OnTimeout(std::chrono::seconds(1), []() {
                printf("!!!FAILED: future is timeout!\n");
                },
                &loop
         );

    // exit after 3 seconds
    loop.ScheduleAfter(std::chrono::seconds(3), []() {
            printf("BYE: now exiting!\n");
            EventLoop::ExitApplication();
            });

    loop.Run();

    t.join();
    return 0;
}

