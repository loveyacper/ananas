
#include <iostream>
#include <string>

#include "Logger.h"
#include "Threads/ThreadPool.h"
#include "Util/TimeUtil.h"

int g_threads = 1;
int g_logs = 100 * 10000;

std::atomic<int> g_complete(0);


int main(int ac, char* av[]) {
    if (ac > 1) {
        g_threads = std::stoi(av[1]);
        if (ac > 2) {
            g_logs = std::stoi(av[2]);
        }
    }
    else {
        printf("usage: ./a.out #g_threads #logcount\n");
        return -1;
    }

    if (g_threads > 100)
        g_threads = 100;
    else if (g_threads <= 0)
        g_threads = 1;

    LogManager::Instance().Start();

    Time start;

    for (int i = 0; i < g_threads; ++ i) {
        Logger* log = LogManager::Instance().CreateLog(logDEBUG | logERROR, logFile, "logdir");
        ThreadPool::Instance().ExecuteTask([=]() {
                for (int n = 0; n < (g_logs / g_threads); n ++) {
                    DBG(log) << n << "|abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxy|";
                }

                ++ g_complete;
         });
    }

    // 等待生产者线程结束
    while (g_complete < g_threads) {
        // 简单sleep等待
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 当生产者线程结束的时候shutdown
    LogManager::Instance().Stop();

    Time end;
    printf("used %lu ms, ", end - start);
    printf("%dw logs/second\n", int(g_logs * 1000.0 / (end - start) / 10000));

    ThreadPool::Instance().JoinAll();

    return 0;
}

