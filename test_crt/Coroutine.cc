#include <cassert>
#include <stdexcept>
#include <iostream>
#include <string>
#include "Future.h"
#include "Coroutine.h"


using std::cerr;
using std::endl; // for test

unsigned int Coroutine::s_id = 0;
Coroutine    Coroutine::main_;
Coroutine*   Coroutine::current_ = 0;


Coroutine::Coroutine(const Function& func, std::size_t  size) 
    : id_( ++ s_id),
    state_(State_init)
#if defined(__gnu_linux__) || defined(__APPLE__)
    ,stack_(size ? size : kDefaultStackSize)
#endif
{
    if (this == &main_)
    {
#if defined(__gnu_linux__) || defined(__APPLE__)
        std::vector<char>().swap(stack_);
#endif
        return;
    }

    if (id_ == main_.id_)
        id_ = ++ s_id;  // when s_id overflow

#if defined(__gnu_linux__) || defined(__APPLE__)
    int ret = ::getcontext(&handle_);
    assert (ret == 0);

    handle_.uc_stack.ss_sp   = &stack_[0];
    handle_.uc_stack.ss_size = stack_.size();
    handle_.uc_link = 0;

    ::makecontext(&handle_, reinterpret_cast<void (*)(void)>(&Coroutine::_Run), 1, this);

#else
    handle_ = ::CreateFiberEx(0, 0, FIBER_FLAG_FLOAT_SWITCH,
                               reinterpret_cast<PFIBER_START_ROUTINE>(&Coroutine::_Run), this);
#endif

    func_  = func;
}

Coroutine::~Coroutine()
{
    cerr << "delete coroutine " << id_ << endl;

#if !defined(__gnu_linux__) && !defined(__APPLE__)
    if (handle_ != INVALID_HANDLE_VALUE)
    {
        ::DeleteFiber(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
#endif
}

std::shared_ptr<void> Coroutine::_Send(Coroutine* crt, std::shared_ptr<void> param)
{
    if (!crt)  return 0;

    assert(this == current_);
    assert(this != crt);

    current_ = crt;

    if (param) {
        this->outputParams_ = param; // set old coroutine's out param
        crt->inputParams_ = param;
    }

#if defined(__gnu_linux__) || defined(__APPLE__)
    int ret = ::swapcontext(&handle_, &crt->handle_);
    if (ret != 0) {
        perror("FATAL ERROR: swapcontext");
    }

#else
    ::SwitchToFiber(crt->handle_);

#endif

    return std::move(crt->outputParams_); // only retrieve once
}

std::shared_ptr<void> Coroutine::_Yield(const std::shared_ptr<void>& param)
{
    return _Send(&main_, param);
}

void Coroutine::_Run(Coroutine* crt)
{
    assert (&Coroutine::main_ != crt);
    assert (Coroutine::current_ == crt);

    cerr << "\n=========== Start croutine id "
         << crt->GetID() << endl;

    crt->state_ = State_running;

    if (crt->func_)
        crt->func_(crt->inputParams_, crt->outputParams_);

    cerr << "=========== Finish croutine id "
         << crt->GetID() << endl << endl;

    crt->inputParams_.reset();
    crt->state_ = State_finish;
    crt->_Yield(crt->outputParams_);
}

CoroutinePtr  CoroutineMgr::CreateCoroutine(const Coroutine::Function& func) 
{
    CoroutinePtr crt(new Coroutine(func)); 
    coroutines_.insert({crt->GetID(), crt});
    return crt;
}

CoroutinePtr  CoroutineMgr::_FindCoroutine(unsigned int id) const
{
    CoroutineMap::const_iterator  it(coroutines_.find(id));

    if (it != coroutines_.end())
        return  it->second;

    return  CoroutinePtr();
}

std::shared_ptr<void> CoroutineMgr::Send(unsigned int id, std::shared_ptr<void> param)
{
    assert (id != Coroutine::main_.id_);
    return  Send(_FindCoroutine(id), param);
}

std::shared_ptr<void> CoroutineMgr::Send(const CoroutinePtr& crt, std::shared_ptr<void> param)
{
    if (crt->state_ == Coroutine::State_finish)
    {
        coroutines_.erase(crt->GetID());
        cerr << "Erase " << crt->GetID() << endl;
        throw std::runtime_error("runtime_error: send data to a finished coroutine!");
    }

    if (!Coroutine::current_)
    {
        Coroutine::current_ = &Coroutine::main_;

#if !defined(__gnu_linux__) && !defined(__APPLE__)
        Coroutine::main_.handle_ = ::ConvertThreadToFiberEx(&Coroutine::main_, FIBER_FLAG_FLOAT_SWITCH);

#endif
    }

    return  Coroutine::current_->_Send(crt.get(), param);
}

std::shared_ptr<void> CoroutineMgr::Yield(const std::shared_ptr<void>& param)
{
    return Coroutine::current_->_Yield(param);
}


CoroutineMgr::~CoroutineMgr()
{
#if !defined(__gnu_linux__) && !defined(__APPLE__)
    if (::GetCurrentFiber() == Coroutine::main_.handle_)
    {
        cerr << "Destroy main fiber\n";
        ::ConvertFiberToThread();
        Coroutine::main_.handle_ = INVALID_HANDLE_VALUE;
    }
    else
    {
        cerr << "What fucking happened???\n";
    }

#endif
}


thread_local CoroutineMgr* CoroutineMgr::s_mgr = nullptr;

CoroutineMgr& CoroutineMgr::Current()
{
    if (!s_mgr)
        s_mgr = new CoroutineMgr; // TODO delete me

    return *s_mgr;
}

// TODO 
void RunCoroutine(const Coroutine::Function& func, const std::shared_ptr<void>& input)
{
    auto& mgr = CoroutineMgr::Current();

    // 1. create coroutine but not run.
    CoroutinePtr crt = mgr.CreateCoroutine(func);

    cerr << "Create Croutine\n";
    bool run = true;

    using FutureAny = Future<std::shared_ptr<void> >;
    //std::shared_ptr<FutureAny> yielded; // yielded Future;
    std::shared_ptr<void> yielded; // yielded Future;
    try {
        // 2. prime the coroutine
        yielded = mgr.Send(crt, input); // run coroutine, fut is yield by func
    } catch(const std::runtime_error& e) {
        assert (false); // can not failed at start
        run = false;
    }

    while (run)
    {
        std::cerr << "running \n";
        if (!yielded)
            return;

        // 3. retrieve the yielded future
        auto fut = std::static_pointer_cast<FutureAny>(yielded); //std::shared_ptr<FutureAny>
        yielded.reset();

#if 1
        // 4. when future is done, switch to the coroutine.
        fut->Then([&yielded, &run, &mgr, crt](const std::shared_ptr<void>& resp) {
                try {
                    std::cerr << "mgr.Send " << (void*)resp.get() << std::endl;
                    yielded = mgr.Send(crt, resp); // future is done, awaken the coroutine with the response
                } catch(...) {
                    std::cerr << "runtime_error Coroutine is over\n";
                    yielded.reset();
                    run = false;
                }
                //return 12345678;
                });
#endif
            // fut->timeOut([&run]() {
            //       run = false;
            //  })
    }
}

