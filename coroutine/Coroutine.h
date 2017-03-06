#ifndef BERT_COROUTINE_H
#define BERT_COROUTINE_H

// Only linux

#include <ucontext.h>

#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace ananas
{
    
using AnyPointer = std::shared_ptr<void>;

class Coroutine
{
    friend class CoroutineMgr;

    enum class State
    {
        Init,
        Running,
        Finish,
    };

public:
    // !!!
    // NEVER define coroutine object, please use CoroutineMgr::CreateCoroutine.
    // Coroutine constructor should be private, 
    // BUT compilers demand template constructor must be public...
    explicit
    Coroutine(std::size_t stackSize = 0);

    // if F return void
    template <typename F, typename... Args, 
              typename = typename std::enable_if<std::is_void<typename std::result_of<F (Args...)>::type>::value, void>::type, typename Dummy = void>
    Coroutine(F&& f, Args&&... args) : Coroutine(kDefaultStackSize)
    {
        auto temp = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        func_ =  [temp] () { (void)temp(); };
    }

    // if F return non-void
    template <typename F, typename... Args, 
              typename = typename std::enable_if<!std::is_void<typename std::result_of<F (Args...)>::type>::value, void>::type>         
    Coroutine(F&& f, Args&&... args) : Coroutine(kDefaultStackSize)
    {
        using ResultType = typename std::result_of<F (Args...)>::type;

        auto me = this;
        auto temp = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        func_ = [temp, me] () mutable {
            me->result_ = std::make_shared<ResultType>(temp());
        };
    }

    ~Coroutine();

    // no copyable
    Coroutine(const Coroutine&) = delete;
    void operator=(const Coroutine&) = delete;
    // no movable
    Coroutine(Coroutine&&) = delete;
    void operator=(Coroutine&&) = delete;

    unsigned int GetID() const  { return  id_; }
    static unsigned int GetCurrentID()  {  return current_->id_; } 

private:
    AnyPointer _Send(Coroutine* crt, AnyPointer = AnyPointer(nullptr));
    AnyPointer _Yield(const AnyPointer& = AnyPointer(nullptr));
    static void _Run(Coroutine* cxt);

    unsigned int id_;  // 1: main
    State state_;
    AnyPointer yieldValue_;

    typedef ucontext HANDLE;

    static const std::size_t kDefaultStackSize = 8 * 1024; 
    std::vector<char> stack_;

    HANDLE handle_;
    std::function<void ()> func_;
    AnyPointer result_;

    static Coroutine main_;
    static Coroutine* current_; 
    static unsigned int sid_;
};

using CoroutinePtr = std::shared_ptr<Coroutine>;

class CoroutineMgr
{
public:
    // works like python decorator: convert the func to a coroutine
    template <typename F, typename... Args>
    static CoroutinePtr
    CreateCoroutine(F&& f, Args&&... args)
    {
        return std::make_shared<Coroutine>(std::forward<F>(f), std::forward<Args>(args)...); 
    }

    // like python generator's send method
    AnyPointer Send(unsigned int id, AnyPointer param = AnyPointer(nullptr));
    AnyPointer Send(const CoroutinePtr& pCrt, AnyPointer = AnyPointer(nullptr));
    static AnyPointer Yield(const AnyPointer& = AnyPointer(nullptr));

    CoroutineMgr(const CoroutineMgr& ) = delete;
    void operator= (const CoroutineMgr& ) = delete;
    CoroutineMgr(CoroutineMgr&& ) = delete;
    void operator= (CoroutineMgr&& ) = delete;

    CoroutineMgr() {}
    ~CoroutineMgr();

private:
    CoroutinePtr _FindCoroutine(unsigned int id) const;

    using CoroutineMap = std::map<unsigned int, CoroutinePtr >;
    CoroutineMap coroutines_;
};

} // end namespace ananas

#endif

