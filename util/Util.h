#ifndef BERT_UTIL_H
#define BERT_UTIL_H

// Ananas tool box

#include <functional>
#include <string>
#include <vector>

namespace ananas {

namespace {

// The defer class for C++11
//
// Usage:
// void f() {
//    FILE* fp = fopen(xxx);
//    if (!fp) return;
//
//    ANANAS_DEFER {
//        // it'll be executed on f() exiting.
//        fclose(fp);
//    }
//
//    ... // Do your business
// }
//
// An example for statics function time cost:
//
// #define STAT_FUNC_COST \
//     // !!! omits std::chrono namespace
//     auto _start_ = steady_clock::now();
//     ANANAS_DEFER {
//          auto end = steady_clock::now();
//          cout << "Used:" << duration_cast<milliseconds>(end-_start_).count();
//     }
//
// // Insert into your function at first line.
// void f() {
//     STAT_FUNC_COST;
//     // when f() exit, will print its running time.
// }
//
class ExecuteOnScopeExit {
public:
    ExecuteOnScopeExit() = default;

    // movable
    ExecuteOnScopeExit(ExecuteOnScopeExit&& ) = default;
    ExecuteOnScopeExit& operator=(ExecuteOnScopeExit&& ) = default;

    // non copyable
    ExecuteOnScopeExit(const ExecuteOnScopeExit& e) = delete;
    void operator=(const ExecuteOnScopeExit& f) = delete;

    template <typename F, typename... Args>
    ExecuteOnScopeExit(F&& f, Args&&... args) {
        func_ = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    }

    ~ExecuteOnScopeExit() noexcept {
        if (func_) func_();
    }

private:
    std::function<void ()> func_;
};

} // end namespace

#define _CONCAT(a, b) a##b
#define _MAKE_DEFER_(line) ananas::ExecuteOnScopeExit _CONCAT(defer, line) = [&]()

#undef ANANAS_DEFER
#define ANANAS_DEFER _MAKE_DEFER_(__LINE__)


inline
std::vector<std::string> SplitString(const std::string& str, char seperator) {
    std::vector<std::string> results;

    std::string::size_type start = 0;
    std::string::size_type sep = str.find(seperator);
    while (sep != std::string::npos) {
        if (start < sep)
            results.emplace_back(str.substr(start, sep - start));

        start = sep + 1;
        sep = str.find(seperator, start);
    }

    if (start != str.size())
        results.emplace_back(str.substr(start));

    return results;
}

} // end namespace ananas

#endif

