#include <iostream>
#include <thread>
#include <chrono>

#include "net/EventLoop.h"
#include "net/Application.h"
#include "util/log/Logger.h"

using namespace ananas;

std::shared_ptr<Logger> log;

int main()
{
    LogManager::Instance().Start();
    log = LogManager::Instance().CreateLog(logALL, logConsole);

    auto& app = Application::Instance();
    auto& loop = *app.BaseLoop();

    loop.Execute([]() {
            INF(log) << "Hello, test timer...";
        });

    // shutdown after 7s
    loop.ScheduleAfter(std::chrono::seconds(7), [&app]() {
            WRN(log) << "Now stop app.";
            app.Exit();
        });

    int count = 0;
    auto forever = loop.ScheduleAfterWithRepeat<kForever>(std::chrono::seconds(1), [&count]() {
            ++ count;
            DBG(log) << "count = " << count << ", you can not see me more than twice.";
        });

    int times = 0;
    auto only5 = loop.ScheduleAfterWithRepeat<5>(std::chrono::seconds(1), [=, &loop, &times]() {
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

    app.Run();

    return 0;
}

