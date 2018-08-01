#include <thread>
#include <iostream>
#include "future/Future.h"

using namespace ananas;


void ThreadFunc(Promise<int>& pm) {
    static std::atomic<int> v{10};
    std::this_thread::sleep_for(std::chrono::milliseconds(80 / v));

    pm.SetValue(v++);
}

int main() {
    std::vector<std::thread> threads;
    std::vector<Promise<int> > pmv(9);
    for (auto& pm : pmv) {
        std::thread t(ThreadFunc, std::ref(pm));
        threads.emplace_back(std::move(t));
    }

    std::vector<Future<int> > futures;
    for (auto& pm : pmv) {
        futures.emplace_back(pm.GetFuture());
    }

    using Type = std::pair<size_t, Try<int>>;

    const int N = static_cast<int>(pmv.size() / 2);
    auto fn = WhenN(N, std::begin(futures), std::end(futures));
    fn.Then([](const std::vector<Type>& results) {
        std::cerr << "Then collet N! goodbye!\n";
        for (auto& t : results)
            std::cerr << t.first << " : " << t.second << std::endl;
    });

    for (auto& t : threads)
        t.join();

    return 0;
}

