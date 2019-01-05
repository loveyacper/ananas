#include <thread>
#include <iostream>
#include "future/Future.h"

using namespace ananas;

template <typename Type>
void ThreadFunc(Promise<Type>& pm) {
    static Type v = 10;
    std::this_thread::sleep_for(std::chrono::milliseconds(v));
    pm.SetValue(v++);
}

void ThreadFuncV(Promise<void>& pm) {
    std::cout << "SetValue Void\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pm.SetValue();
}

void TestWhenAnyIntFuture() {
    std::vector<std::thread> threads;
    std::vector<Promise<int> > pmv(8);
    for (auto& pm : pmv) {
        std::thread t(ThreadFunc<int>, std::ref(pm));
        threads.emplace_back(std::move(t));
    }

    std::vector<Future<int> > futures;
    for (auto& pm : pmv) {
        futures.emplace_back(pm.GetFuture());
    }

    auto fany = WhenAny(std::begin(futures), std::end(futures));
    fany.Then([](const std::pair<size_t, Try<int>>& result) {
        std::cerr << "Then collet int any!\n";
        std::cerr << "Result " << result.first << " = " << result.second << std::endl;
    });

    for (auto& t : threads)
        t.join();
}

void TestWhenAnyVoidFuture() {
    std::vector<std::thread> threads;
    std::vector<Promise<void> > pmv(8);
    for (auto& pm : pmv) {
        std::thread t(ThreadFuncV, std::ref(pm));
        threads.emplace_back(std::move(t));
    }

    std::vector<Future<void>> futures;
    for (auto& pm : pmv) {
        futures.emplace_back(pm.GetFuture());
    }

    auto fany = WhenAny(std::begin(futures), std::end(futures));
    fany.Then([](const std::pair<size_t, Try<void>>& result) {
        std::cerr << "Then collet void any!\n";
        std::cerr << "Result index " << result.first << std::endl;
    });

    for (auto& t : threads)
        t.join();
}

int main() {
    TestWhenAnyIntFuture();
    TestWhenAnyVoidFuture();

    return 0;
}

