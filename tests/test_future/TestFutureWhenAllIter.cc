#include <thread>
#include <iostream>
#include "future/Future.h"

using namespace ananas;

template <typename Type>
void ThreadFunc(Promise<Type>& pm) {
    static std::atomic<Type> v{10};
    std::this_thread::sleep_for(std::chrono::milliseconds(80 / v));
    pm.SetValue(v++);
}

int main() {
    std::vector<std::thread>  threads;
    std::vector<Promise<int> > pmv(9);
    for (auto& pm : pmv) {
        std::thread t(ThreadFunc<int>, std::ref(pm));
        threads.emplace_back(std::move(t));
    }

    std::vector<Future<int> > futures;
    for (auto& pm : pmv) {
        futures.emplace_back(pm.GetFuture());
    }

    auto fall = WhenAll(std::begin(futures), std::end(futures));
    fall.Then([](const std::vector<Try<int>>& results) {
        std::cerr << "Then collet all! goodbye!\n";
        for (auto& t : results)
            std::cerr << t << std::endl;
    });

    for (auto& t : threads)
        t.join();

    return 0;
}

