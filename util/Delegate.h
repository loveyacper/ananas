#ifndef BERT_DELEGATE_H
#define BERT_DELEGATE_H

#include <functional>
#include <list>


namespace ananas {

template <typename T>
class Delegate;

template <typename... Args>
class Delegate<void (Args...)> {
public:
    typedef Delegate<void (Args...)> Self;

    Delegate() = default;

    Delegate(const Self& ) = delete;
    Self& operator= (const Self& ) = delete;

    template <typename F>
    Delegate(F&& f) {
        connect(std::forward<F>(f));
    }

    Delegate(Self&& other) :
        funcs_(std::move(other.funcs_)) {
    }

    template <typename F>
    Self& operator+=(F&& f) {
        connect(std::forward<F>(f));
        return *this;
    }

    template <typename F>
    Self& operator-=(F&& f) {
        disconnect(std::forward<F>(f));
        return *this;
    }

    template<typename... ARGS>
    void operator()(ARGS&&... args) {
        call(std::forward<ARGS>(args)...);
    }

private:
    std::list<std::function<void (Args ...)> > funcs_;

    template <typename F>
    void connect(F&& f) {
        funcs_.emplace_back(std::forward<F>(f));
    }

    template <typename F>
    void disconnect(F&& f) {
        for (auto it(funcs_.begin()); it != funcs_.end(); ++ it) {
            const auto& target = it->template target<decltype(std::addressof(f))>();
            if (target) {
                if (*target == &f) {
                    funcs_.erase(it);
                    return;
                }
            } else {
                const auto& target2 = it->template target<typename std::remove_reference<decltype(f)>::type>();

                // the function object must implement operator ==
                if (target2 && *target2 == f) {
                    funcs_.erase(it);
                    return;
                }
            }
        }
    }

    template <typename... ARGS>
    void call(ARGS&&... args) {
        // But what if rvalue args?
        for (const auto& f : funcs_)
            f(std::forward<ARGS>(args)...);
    }
};

} // end namespace ananas

#endif

