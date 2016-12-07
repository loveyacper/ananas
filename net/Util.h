#ifndef BERT_UTIL_H
#define BERT_UTIL_H

#include <functional>

namespace ananas
{

// The defer class for C++11
class ExecuteOnScopeExit
{
public:
    ExecuteOnScopeExit() { }
    
    ExecuteOnScopeExit(ExecuteOnScopeExit&& e)
    {
        func_ = std::move(e.func_);
    }
    
    ExecuteOnScopeExit(const ExecuteOnScopeExit& e) = delete;
    void operator=(const ExecuteOnScopeExit& f) = delete;
    
    template <typename F, typename... Args>
    ExecuteOnScopeExit(F&& f, Args&&... args)
    {
        auto temp = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        func_ = [temp]() { (void)temp(); };
    }
    
    ~ExecuteOnScopeExit() noexcept
    {
        if (func_)  func_();
    }
    
private:
    std::function<void ()> func_;
};
    
#define CONCAT(a, b) a##b
#define _MAKE_DEFER_HELPER_(line)  ananas::ExecuteOnScopeExit CONCAT(defer, line) = [&]()

#undef DEFER
#define DEFER _MAKE_DEFER_HELPER_(__LINE__)

} // end namespace ananas

#endif

