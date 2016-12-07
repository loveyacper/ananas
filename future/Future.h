#ifndef BERT_FUTURE_H
#define BERT_FUTURE_H

#include <future>
#include <iostream>
#include "Helper.h"
#include "Try.h"

template <typename T>
class State {
public:
    //static_assert(std::is_same<T, void>::value || std::is_copy_constructible<T>(), "must be copyable or void");
    //static_assert(std::is_same<T, void>::value || std::is_move_constructible<T>(), "must be movable or void");

    State() : ft_(pm_.get_future()), retrieved_{false} {// retrieved_(std::ATOMIC_FLAG_INIT) {
    }

//private:
    std::promise<T> pm_;
    std::future<T> ft_;

    std::mutex thenLock_; // Protect then_, it's ineffecient, but I don't want write future from scrath
    std::function<void (Try<T>&& )> then_; // TODO  race!!
    std::atomic<bool> retrieved_;
};

template <typename T>
class Future;

template <typename T>
class Promise
{
public:
    Promise() : state_(std::make_shared<State<T>>()) {
    }

    Promise(const Promise&) = default;
    Promise& operator = (const Promise&) = default;

    Promise(Promise&& pm)
    {
        this->state_ = std::move(pm.state_);
    }
    Promise& operator= (Promise&& pm) 
    {
        if (this != &pm)
            this->state_ = std::move(pm.state_);

        return *this;
    }

    template <typename SHIT = T>
    typename std::enable_if<!std::is_void<SHIT>::value, void>::type
    SetValue(SHIT&& t) {
        state_->pm_.set_value(t);
        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->then_)
            state_->then_(std::move(t));
    }


    template <typename SHIT = T>
    typename std::enable_if<!std::is_void<SHIT>::value, void>::type
    SetValue(const SHIT& t) {
        state_->pm_.set_value(t);
        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->then_)
            state_->then_(t);
    }

    template <typename SHIT = T>
    typename std::enable_if<!std::is_void<SHIT>::value, void>::type
    SetValue(Try<SHIT>&& t) {
        state_->pm_.set_value(t);
        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->then_)
            state_->then_(std::move(t));
    }

    template <typename SHIT = T>
    typename std::enable_if<!std::is_void<SHIT>::value, void>::type
    SetValue(const Try<SHIT>& t) {
        state_->pm_.set_value(t);
        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->then_)
            state_->then_(t);
    }

    template <typename SHIT = T>
    typename std::enable_if<std::is_void<SHIT>::value, void>::type
    SetValue(Try<void>&& ) {
        state_->pm_.set_value();
        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->then_)
            state_->then_(Try<void>());
    }

    template <typename SHIT = T>
    typename std::enable_if<std::is_void<SHIT>::value, void>::type
    SetValue(const Try<void>& ) {
        state_->pm_.set_value();
        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->then_)
            state_->then_(Try<void>());
    }

    template <typename SHIT = T>
    typename std::enable_if<std::is_void<SHIT>::value, void>::type
    SetValue() {
        state_->pm_.set_value();
        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->then_)
            state_->then_(Try<void>());
    }

    Future<T> GetFuture() {
        bool expect = false;
        if (!state_->retrieved_.compare_exchange_strong(expect, true)) {
            struct FutureAlreadyRetrieved {};
            throw FutureAlreadyRetrieved();
        }

        return Future<T>(state_);
    }
private:
    std::shared_ptr<State<T>> state_;
};

template <typename T>
class Future
{
public:
    using InnerType = T;

    Future()
    {
    }

    Future(const Future&) = delete;
    void operator = (const Future&) = delete;

    Future(Future&& fut)
    {
        this->state_ = std::move(fut.state_);
    }

    Future& operator= (Future&& fut)
    {
        if (&fut != this)
            this->state_ = std::move(fut.state_);
        return *this;
    }

    explicit
    Future(std::shared_ptr<State<T>> state) : state_(std::move(state))
    {
    }

    bool IsReady() const {  // can call multi times
        auto res = this->WaitFor(std::chrono::seconds(0));
        std::cout << "Is Ready " << (res == std::future_status::ready) << std::endl;
        return res == std::future_status::ready;
    }

    template <typename SHIT = T>
    typename std::enable_if<!std::is_void<SHIT>::value, T>::type
    GetValue() { // only once
        return state_->ft_.get();
    }

    template <typename SHIT = T>
    typename std::enable_if<std::is_void<SHIT>::value, Try<void> >::type
    GetValue() {
        state_->ft_.get();
        return Try<void>();
    }

    void Wait() const {
        state_->ft_.wait();
    }
      
    template<typename R, typename P>
    std::future_status
    WaitFor(const std::chrono::duration<R, P>& dur) const {
        return state_->ft_.wait_for(dur);
    }

    template<typename Clock, typename Duration>
    std::future_status
    WaitUntil(const std::chrono::time_point<Clock, Duration>& abs) const {
        return state_->ft_.wait_until(abs);
    }

    template <typename F,
              typename R = CallableResult<F, T> > // f(T&)
    auto Then(F&& f) -> typename R::ReturnFutureType  {
        typedef typename R::Arg Arguments; // 这是func的返回类型的包装
        return _ThenImpl<F, R>(std::forward<F>(f), Arguments());  
    }

    //1. F not return future type
    template <typename F, typename R, typename... Args>
    typename std::enable_if<!R::IsReturnsFuture::value, typename R::ReturnFutureType>::type
    _ThenImpl(F&& f, ResultOfWrapper<F, Args...> ) {
        static_assert(sizeof...(Args) <= 1, "Then must take zero/one argument");
        using FReturnType = typename R::IsReturnsFuture::Inner;

        //static_assert(std::is_same<FReturnType, void>::value, "fuck me");

        Promise<FReturnType> pm;
        auto nextFuture = pm.GetFuture();

        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (IsReady()) {
            Try<T> t(GetValue());
            auto result = WrapWithTry(f, t.template Get<Args>()...);
            pm.SetValue(std::move(result));
        }
        else {
#if 1
            SetCallback([func = std::move(f), prom = std::move(pm)](Try<T>&& t) mutable {
                // run callback
                auto result = WrapWithTry(func, t.template Get<Args>()...); // T can be void, thanks to folly Try<>
                // set next future
                prom.SetValue(std::move(result));
            });
#else
         #if 1
            // work around for C++11: no move capture for lambda, suck...
            auto func = (std::bind([func = f](Try<T>&& t, Promise<FReturnType>& prom) mutable {
                    // run callback
                    auto result = WrapWithTry(func, t.template Get<Args>()...); // T can be void, thanks to folly Try<>
                    // set next future
                    prom.SetValue(std::move(result));
                },
                std::placeholders::_1,
                std::move(pm)
            ));
            //decltype(func) f2 = std::move(func);  //OK
            SetCallback(std::move(func));
        #endif
#endif
        }

        return std::move(nextFuture);
    }

#if 1
    //2. F return another future type
    template <typename F, typename R, typename... Args>
    typename std::enable_if<R::IsReturnsFuture::value, typename R::ReturnFutureType>::type
    _ThenImpl(F&& f, ResultOfWrapper<F, Args...>) {
        static_assert(sizeof...(Args) <= 1, "Then must take zero/one argument");
        using FReturnType = typename R::IsReturnsFuture::Inner;

        Promise<FReturnType> pm;
        auto nextFuture = pm.GetFuture();

        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (IsReady()) {
            Try<T> t(GetValue());
            auto f2 = f(t.template Get<Args>()...);
            f2.SetCallback([p2 = std::move(pm)](Try<FReturnType>&& b) mutable {
                p2.SetValue(std::move(b));
            });  
        }
        else {
            SetCallback([func = std::move(f), prom = std::move(pm)](Try<T>&& t) mutable {
                // because funcm return another future:f2, when f2 is done, nextFuture can be done
                auto f2 = func(t.template Get<Args>()...);
                f2.SetCallback([p2 = std::move(prom)](Try<FReturnType>&& b) mutable {
                    p2.SetValue(std::move(b));
                });
            }); 
        }

        return std::move(nextFuture);
    }
#endif

    void SetCallback(decltype(std::declval<State<T>>().then_)&& func)
    {
        this->state_->then_ = std::move(func);
    }

    void SetCallback(const decltype(std::declval<State<T>>().then_)& func)
    {
        //this->state_->then_ = func;
    }
private:
    std::shared_ptr<State<T>> state_;
};



template <typename... FT>
typename CollectAllVariadicContext<typename std::decay<FT>::type::InnerType...>::FutureType
WhenAll(FT&&... futures)
{
    auto ctx = std::make_shared<CollectAllVariadicContext<typename std::decay<FT>::type::InnerType...>>();

    CollectVariadicHelper<CollectAllVariadicContext>( 
            ctx, std::forward<typename std::decay<FT>::type>(futures)...);

    return ctx->pm.GetFuture();
}


template <class InputIterator>
Future<
    std::vector<
    Try<typename std::iterator_traits<InputIterator>::value_type::InnerType>>>
    WhenAll(InputIterator first, InputIterator last)
{
    using T = typename std::iterator_traits<InputIterator>::value_type::InnerType;

    struct CollectAllContext
    {
        CollectAllContext(int n) : results(n) {}
        ~CollectAllContext()
        { 
//            pm.SetValue(std::move(results));
        }    
             
        Promise<std::vector<Try<T>>> pm;
        std::vector<Try<T>> results;
        std::vector<size_t> collects;

        bool IsReady() const { return results.size() == collects.size(); }
    };

    auto ctx = std::make_shared<CollectAllContext>(std::distance(first, last));
                
    for (size_t i = 0; first != last; ++first, ++i)
    {
        first->SetCallback([ctx, i](Try<T>&& t) {
                ctx->results[i] = std::move(t);
                ctx->collects.push_back(i);
                std::cerr << "Collect size = " << ctx->collects.size() << std::endl;
                std::cerr << "Result size = " << ctx->results.size() << std::endl;
                if (ctx->IsReady()) 
                    ctx->pm.SetValue(std::move(ctx->results));
                });
    }
    return ctx->pm.GetFuture();
}

template <class InputIterator>
Future<
  std::pair<size_t,
              Try<typename std::iterator_traits<InputIterator>::value_type::InnerType>>>
WhenAny(InputIterator first, InputIterator last)
{
    using T = typename std::iterator_traits<InputIterator>::value_type::InnerType;

    struct CollectAnyContext
    {
        CollectAnyContext() {};
        Promise<std::pair<size_t, Try<T>>> pm;
        std::atomic<bool> done{false};
    };

    auto ctx = std::make_shared<CollectAnyContext>();
    for (size_t i = 0; first != last; ++first, ++i)
    {
        first->SetCallback([ctx, i](Try<T>&& t) {
            if (!ctx->done.exchange(true))
            {
                std::cerr << "set done = " << i << ": " << t << std::endl;
                ctx->pm.SetValue(std::make_pair(i, std::move(t)));
            }
       });
    }
           
    return ctx->pm.GetFuture();
}

#endif

