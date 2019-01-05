#include <thread>
#include <iostream>
#include "future/Future.h"

using namespace ananas;


void ThreadFunc(Promise<int>& pm) {
    static std::atomic<int> v{1};
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

    // N == 4
    const int N = static_cast<int>(pmv.size() / 2);
    // expect odd number, so it'll be 1,3,5,7
    auto cond = [](const Try<int>& v)->bool {
        return v % 2 == 1;
    };
    WhenIfN(N, std::begin(futures), std::end(futures), cond)
      .Then([N](Try<std::vector<Type>>&& results) {
        try {
            std::vector<Type> res = std::move(results);
            std::cerr << "Then collet " << N << "! goodbye!\n";
            // expect 1,3,5,7
            for (auto& t : res)
                std::cerr << t.first << " : " << t.second << std::endl;
        } catch(const std::exception& e) {
            std::cerr << e.what() << ", Then collet " << N << " FAILED ! goodbye!\n";
        }
    });

    for (auto& t : threads)
        t.join();

    return 0;
}

