#include <iostream>
#include <thread>
#include <chrono>
#include "net/EventLoop.h"
#include "net/ThreadPool.h"
#include "net/log/Logger.h"

using namespace ananas;

std::shared_ptr<Logger> log;

int main()
{
    LogManager::Instance().Start();
    log = LogManager::Instance().CreateLog(logALL, logConsole);

    EventLoop& loop = g_eventloop.Instance();

    loop.ScheduleNextTick([]() {
            INF(log) << "Hello, test timer...";
        });

    // shutdown after 7s
    loop.ScheduleAfter(std::chrono::seconds(7), [&loop]() {
            WRN(log) << "Now stop app.";
            EventLoop::ExitAll();
        });

    int count = 0;
    auto forever = loop.ScheduleAfter<kForever>(std::chrono::seconds(1), [&count]() {
            ++ count;
            DBG(log) << "count = " << count << ", you can not see me more than twice.";
        });

    int times = 0;
    auto only5 = loop.ScheduleAfter<5>(std::chrono::seconds(1), [=, &loop, &times]() {
            ++ times;
            DBG(log) << "Trigger every 1s, the " << times << " time";
            if (times == 2)
            {
                if (loop.Cancel(forever))
                    WRN(log) << "Cancel timer";
                else
                    ERR(log) << "BUG: Cancel failed!!!";
            }
        });

    (void)only5;

    loop.Run();

    return 0;
}

