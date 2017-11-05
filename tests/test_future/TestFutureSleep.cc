#include <thread>
#include <cstdio>
#include "net/EventLoop.h"

using namespace ananas;

int main()
{
    ananas::EventLoop loop;

    // exit after sleep 5 seconds
    loop.Sleep(std::chrono::seconds(5))
        .Then([]() {
            printf("After sleep for 5 seconds, exit!\n");
            ananas::EventLoop::ExitApplication();
        });

    loop.ScheduleAfter<kForever>(std::chrono::seconds(1), []() {
        printf("waiting for exit...\n");
    });

    loop.Run();

    return 0;
}

