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

    // expect odd number, so it may be 1,3,5,7
    std::function<bool (const Try<int>& )> cond = [](const Try<int>& v)->bool {
        return v % 2 == 1;
    };
    WhenIfAny(std::begin(futures), std::end(futures), cond)
      .Then([](Try<Type>&& result) {
        try {
            Type t = std::move(result);
            std::cerr << "Then when any if, goodbye!\n";
            // expect may be 1,3,5,7
            std::cerr << "Result index " << t.first << ", value " << t.second << std::endl;
        } catch(const std::exception& e) {
            std::cerr << e.what() << ", Then when if any FAILED ! goodbye!\n";
        }
    });

    for (auto& t : threads)
        t.join();

    return 0;
}

