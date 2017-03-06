#include <cassert>
#include <stdexcept>
#include <string>
#include "Coroutine.h"

namespace ananas
{

unsigned int Coroutine::sid_ = 0;
Coroutine Coroutine::main_;
Coroutine* Coroutine::current_ = nullptr;

Coroutine::Coroutine(std::size_t size) :
    id_( ++ sid_),
    state_(State::Init),
    stack_(size > kDefaultStackSize ? size : kDefaultStackSize)
{
    if (this == &main_)
    {
        std::vector<char>().swap(stack_);
        return;
    }

    if (id_ == main_.id_)
        id_ = ++ sid_;  // when sid_ overflow

    int ret = ::getcontext(&handle_);
    assert (ret == 0);

    handle_.uc_stack.ss_sp   = &stack_[0];
    handle_.uc_stack.ss_size = stack_.size();
    handle_.uc_link = 0;

    ::makecontext(&handle_, reinterpret_cast<void (*)(void)>(&Coroutine::_Run), 1, this);
}

Coroutine::~Coroutine()
{
}

AnyPointer Coroutine::_Send(Coroutine* crt, AnyPointer param)
{
    assert (crt);

    assert(this == current_);
    assert(this != crt);

    current_ = crt;

    if (param)
    {
        // just behave like python's generator
        if (crt->state_ == State::Init && crt != &Coroutine::main_)
            throw std::runtime_error("Can't send non-void value to a just-created coroutine");

        // set old coroutine's yield value
        this->yieldValue_ = std::move(param);
    }

    int ret = ::swapcontext(&handle_, &crt->handle_);
    if (ret != 0)
    {
        perror("FATAL ERROR: swapcontext");
        throw std::runtime_error("FATAL ERROR: swapcontext failed");
    }

    return std::move(crt->yieldValue_); // only return once
}

AnyPointer Coroutine::_Yield(const AnyPointer& param)
{
    return _Send(&main_, param);
}

void Coroutine::_Run(Coroutine* crt)
{
    assert (&Coroutine::main_ != crt);
    assert (Coroutine::current_ == crt);

    crt->state_ = State::Running;

    if (crt->func_)
        crt->func_();

    crt->state_ = State::Finish;
    crt->_Yield(crt->result_);
}


CoroutinePtr  CoroutineMgr::_FindCoroutine(unsigned int id) const
{
    CoroutineMap::const_iterator  it(coroutines_.find(id));

    if (it != coroutines_.end())
        return it->second;

    return CoroutinePtr();
}

AnyPointer CoroutineMgr::Send(unsigned int id, AnyPointer param)
{
    assert (id != Coroutine::main_.id_);
    return Send(_FindCoroutine(id), param);
}

AnyPointer CoroutineMgr::Send(const CoroutinePtr& crt, AnyPointer param)
{
    if (crt->state_ == Coroutine::State::Finish)
    {
        throw std::runtime_error("Send to a finished coroutine.");
        return AnyPointer();
    }

    if (!Coroutine::current_)
    {
        Coroutine::current_ = &Coroutine::main_;
    }

    return Coroutine::current_->_Send(crt.get(), param);
}

AnyPointer CoroutineMgr::Yield(const AnyPointer& param)
{
    return Coroutine::current_->_Yield(param);
}


CoroutineMgr::~CoroutineMgr()
{
}

} // end namespace ananas

