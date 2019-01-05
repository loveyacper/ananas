#include <thread>
#include <iostream>
#include "future/Future.h"

using namespace ananas;


void ThreadFunc(Promise<int>& pm) {
    static std::atomic<int> v{10};
    std::this_thread::sleep_for(std::chrono::milliseconds(v));
    pm.SetValue(v++);
}

void ThreadFuncV(Promise<void>& pm) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    pm.SetValue();
}

void TestWhenNInt() {
    std::vector<std::thread> threads;
    std::vector<Promise<int>> pmv(9);
    for (auto& pm : pmv) {
        std::thread t(ThreadFunc, std::ref(pm));
        threads.emplace_back(std::move(t));
    }

    std::vector<Future<int>> futures;
    for (auto& pm : pmv) {
        futures.emplace_back(pm.GetFuture());
    }

    using Type = std::pair<size_t, Try<int>>;

    const int N = static_cast<int>(pmv.size() / 2);
    auto fn = WhenN(N, std::begin(futures), std::end(futures));
    fn.Then([](const std::vector<Type>& results) {
        std::cerr << "Then collet N int!" << std::endl;
        for (auto& t : results)
            std::cerr << t.first << " : " << t.second << std::endl;
    });

    for (auto& t : threads) {
        t.join();
    }
}

void TestWhenNVoid() {
    std::vector<std::thread> threads;
    std::vector<Promise<void>> pmv(9);
    for (auto& pm : pmv) {
        std::thread t(ThreadFuncV, std::ref(pm));
        threads.emplace_back(std::move(t));
    }

    std::vector<Future<void>> futures;
    for (auto& pm : pmv) {
        futures.emplace_back(pm.GetFuture());
    }

    using Type = std::pair<size_t, Try<void>>;

    const int N = static_cast<int>(pmv.size() / 2);
    auto fn = WhenN(N, std::begin(futures), std::end(futures));
    fn.Then([](const std::vector<Type>& results) {
        std::cerr << "Then collet N void!" << std::endl;
        for (auto& t : results)
            std::cerr << "Index : " << t.first << std::endl;
    });

    for (auto& t : threads) {
        t.join();
    }
}

int main() {
    TestWhenNInt();
    TestWhenNVoid();
    return 0;
}

