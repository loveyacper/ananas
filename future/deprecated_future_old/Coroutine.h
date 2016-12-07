#ifndef BERT_COROUTINE_H
#define BERT_COROUTINE_H

// Only linux & windows, MAC OS do not support swapcontext, TODO

#if defined(__APPLE__)
#define _XOPEN_SOURCE
#define thread_local __thread
#endif

#if defined(__gnu_linux__) || defined(__APPLE__)
#include <ucontext.h>

#else   // windows
#include <Windows.h>
#undef    Yield // cancel the windows macro

#endif

#include <vector>
#include <map>
#include <memory>
#include <functional>

class  Coroutine
{
    friend class CoroutineMgr;

    enum State
    {
        State_init,
        State_running,
        State_finish,
    };

public:
    typedef  std::function<void (const std::shared_ptr<void>&, std::shared_ptr<void>& )>  Function;

    ~Coroutine();
    Coroutine(const Coroutine&) = delete;
    void operator=(const Coroutine&) = delete;

    unsigned int GetID() const  { return  id_; }
    static unsigned int GetCurrentID()  {  return current_->id_; } 

private:
    Coroutine(const Function& func = Function(), std::size_t size = 0);

    std::shared_ptr<void> _Send(Coroutine* crt, std::shared_ptr<void> inParam = std::shared_ptr<void>(nullptr));
    std::shared_ptr<void> _Yield(const std::shared_ptr<void>& = std::shared_ptr<void>(nullptr));
    static void _Run(Coroutine* cxt);

    unsigned int id_;  // 1: main
    int state_;
    std::shared_ptr<void> inputParams_;
    std::shared_ptr<void> outputParams_;

#if defined(__gnu_linux__) 
    typedef ucontext HANDLE;

    static const std::size_t kDefaultStackSize = 8 * 1024; 
    std::vector<char> stack_;

#elif defined(__APPLE__)
    typedef ucontext_t HANDLE;

    static const std::size_t kDefaultStackSize = 8 * 1024; 
    std::vector<char> stack_;
#else
    typedef PVOID HANDLE;

#endif

    HANDLE handle_;
    Function func_;

    static Coroutine main_;
    static Coroutine* current_; 
    static unsigned int s_id;
};

using CoroutinePtr = std::shared_ptr<Coroutine>;

class CoroutineMgr
{
public:
    // works like python decorator: convert the func to a coroutine
    CoroutinePtr CreateCoroutine(const Coroutine::Function& func);

    // CoroutineMgr per thread
    static CoroutineMgr& Current();

    // like python generator's send method
    std::shared_ptr<void>   Send(unsigned int id, std::shared_ptr<void> param = std::shared_ptr<void>(nullptr));
    std::shared_ptr<void>   Send(const CoroutinePtr& crt, std::shared_ptr<void> param = std::shared_ptr<void>(nullptr));
    static std::shared_ptr<void> Yield(const std::shared_ptr<void>& = std::shared_ptr<void>(nullptr));

    CoroutineMgr(const CoroutineMgr& ) = delete;
    void operator = (const CoroutineMgr& ) = delete;

    ~CoroutineMgr();

private:
    CoroutineMgr() = default;

    CoroutinePtr  _FindCoroutine(unsigned int id) const;

    typedef std::map<unsigned int, CoroutinePtr > CoroutineMap;
    CoroutineMap coroutines_;

    static thread_local CoroutineMgr* s_mgr;
};

void RunCoroutine(const Coroutine::Function& func, const std::shared_ptr<void>& input);

#endif

