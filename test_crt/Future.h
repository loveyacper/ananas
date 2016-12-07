#ifndef BERT_FUTURE_H
#define BERT_FUTURE_H

#include <future>
#include <iostream>

// simple Future with callback
static int count = 0;

template <typename T>
class Future
{
public:
    Future()
    {
        ++ count;
        std::cerr << __FUNCTION__ << (void*)this <<  std::endl;
        inner_ = future_.get_future();
    }
    ~Future()
    {
        -- count;
        std::cerr << __FUNCTION__ << count <<  std::endl;
    }
    Future(const T& t)
    {
        ++ count;
        std::cerr << __FUNCTION__ << (void*)this <<  std::endl;
        inner_ = future_.get_future();
        this->SetValue(t);
    }

    Future(Future&& fut)
    {
        ++ count;
        std::cerr << __FUNCTION__ << (void*)this <<  std::endl;
        this->_MoveFrom(std::move(fut));
    }
      
    Future& operator=(Future&& fut)
    {
        ++ count;
        std::cerr << __FUNCTION__ << (void*)this <<  std::endl;
        this->_MoveFrom(std::move(fut));
        return *this;
    }
      
    bool Ready()
    {
        // can call multi times
        std::cerr << "Test ready for " << (void*) this << std::endl;
        auto res = inner_.wait_for(std::chrono::seconds(0));
        return res == std::future_status::ready;
    }

    T GetValue()
    {
        // only one time
        std::cerr << "Try get value " << (void*) this << std::endl;
        return inner_.get();
    }

    // Disable copying
    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;

    // Then
    template <typename F>
#if 0
    auto Then(F&& f) -> Future<typename std::result_of<F (const T& )>::type>&
    {
        std::cerr << __FUNCTION__ << (void*)this <<  std::endl;
        using ReturnType = typename std::result_of<F (const T& )>::type;

        // return another future!  
        auto nextFut = std::make_shared<Future<ReturnType> >();

        if (Ready())
        {
            std::cerr << "It is ready\n";
            ReturnType res(f(GetValue()));

            this->then_ = [nextFut] (const T& ) {
                (void)nextFut;
            };
            nextFut->SetValue(res);
        }
        else
        {
            auto thenT = std::make_shared<std::packaged_task<ReturnType (const T& )> >(std::bind(std::forward<F>(f),
                                                                                       std::placeholders::_1));
            auto stdFuture(thenT->get_future());

            // C++11 does NOT support move capture for lambda
            this->then_ = std::bind( [thenT, nextFut] (const std::future<ReturnType>& stdFut, const T& t) {
                        (*thenT)(t);
                        // if ReturnType != void
                        nextFut->SetValue(const_cast<std::future<ReturnType>& >(stdFut).get());
                    }
                    , std::cref(stdFuture)
                    , std::placeholders::_1
                    );
        }

        return *nextFut;
    }
#else
    void Then(F&& f)
    {
        std::cerr << __FUNCTION__ << (void*)this <<  std::endl;
        if (Ready())
        {
            std::cerr << "It is ready\n";
            // TODO if ready, call it asap!
            f(GetValue());
            return;
        }
    
        // C++11 does NOT support move capture for lambda
        this->then_ = std::bind([f] (const T& t) {
                    f(t);
                }
                , std::placeholders::_1
                );
    }
#endif

    // Setting the result
    void SetValue(const T& t)
    {
        std::cerr << __FUNCTION__ << (void*)this <<  std::endl;
        future_.set_value(t);
        if (then_) 
        {
            then_(t);

            decltype(then_) tmp; 
            tmp.swap(then_);
        }
    }

    void SetValue(T&& t)
    {
        std::cerr << __FUNCTION__ << (void*)this <<  std::endl;
        future_.set_value(std::move(t));
        if (then_) 
        {
            then_(inner_.get());

            decltype(then_) tmp; 
            tmp.swap(then_);
        }
    }

private:
    void _MoveFrom(Future&& fut)
    {
        if (&fut == this)
            return;

        future_ = std::move(fut.future_);
        inner_ = std::move(fut.inner_);
        then_ = std::move(fut.then_);
    }
      
    std::promise<T> future_; // YES, I must use set_value();
    std::future<T> inner_;
    std::function<void (T const &)> then_;
    // TODO timeout & error
};


template <>
class Future<void>
{
public:
    Future()
    {
        ++ count;
        std::cerr << __FUNCTION__ << (void*)this <<  std::endl;
        inner_ = future_.get_future();
    }
    ~Future()
    {
        -- count;
        std::cerr << __FUNCTION__ << count <<  std::endl;
    }

    Future(Future&& fut)
    {
        ++ count;
        std::cerr << __FUNCTION__ << (void*)this <<  std::endl;
        this->_MoveFrom(std::move(fut));
    }
      
    Future& operator=(Future&& fut)
    {
        ++ count;
        std::cerr << __FUNCTION__ << (void*)this <<  std::endl;
        this->_MoveFrom(std::move(fut));
        return *this;
    }
      
    bool Ready()
    {
        // can call multi times
        std::cerr << "Test ready for " << (void*) this << std::endl;
        auto res = inner_.wait_for(std::chrono::seconds(0));
        return res == std::future_status::ready;
    }

    void GetValue()
    {
        // only one time
        std::cerr << "Try get value " << (void*) this << std::endl;
        inner_.get();
    }

    // Disable copying
    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;

    // Then
    template <typename F>
    void Then(F&& f)
    {
        std::cerr << __FUNCTION__ << (void*)this <<  std::endl;
        //using ReturnType = typename std::result_of<F (const T& )>::type;
        if (Ready())
        {
            std::cerr << "It is ready\n";
            // TODO if ready, call it asap!
            f(GetValue());
            return;
        }
    
        // C++11 does NOT support move capture for lambda
        this->then_ = f;
    }

    // Setting the result
    void SetValue()
    {
        std::cerr << __FUNCTION__ << (void*)this <<  std::endl;
        future_.set_value();
        if (then_) 
        {
            then_();

            decltype(then_) tmp; 
            tmp.swap(then_);
        }
    }

private:
    void _MoveFrom(Future&& fut)
    {
        if (&fut == this)
            return;

        future_ = std::move(fut.future_);
        inner_ = std::move(fut.inner_);
        then_ = std::move(fut.then_);
    }
      
    std::promise<void> future_; // YES, I must use set_value();
    std::future<void> inner_;
    std::function<void ()> then_;
    // TODO timeout & error
};

#endif

