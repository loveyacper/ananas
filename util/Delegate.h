#ifndef BERT_DELEGATE_H
#define BERT_DELEGATE_H

#include <functional>
#include <list>


///@file Delegate.h
///@brief A delegate class like C#
namespace ananas {

template <typename T>
class Delegate;

///@brief A delegate class like C# delegate
/// Usage
///@code
///  //三个函数的定义
///  void Inc(int& n) { ++n; }
///  void Print(int& n) { cout << n << endl; }
///
///  class Test {
///  public:
///      void MInc(int& n) { ++n; }
///  };
///
///  //测试委托
///  int n = 0;
///  ananas::Delegate<void (int& )> cb;
///  cb += Inc;
///  cb += Print;
///  cb(n);  // 先执行Inc,再执行print打印1
///
///  Test obj;
///  cb += std::bind(&Test::MInc, &obj, std::placeholders::_1); //也可以注册成员函数
///  //注册lambda也没问题,只要签名一致
///  cb += [](int& b) { ++b; };
///@endcode

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

    template <typename... ARGS>
    void call(ARGS&&... args) {
        // But what if rvalue args?
        for (const auto& f : funcs_)
            f(std::forward<ARGS>(args)...);
    }
};

} // end namespace ananas

#endif

