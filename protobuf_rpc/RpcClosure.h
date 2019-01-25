#ifndef BERT_RPCCLOSURE_H
#define BERT_RPCCLOSURE_H

#include <functional>
#include <type_traits>

#include <google/protobuf/stubs/callback.h>

// A much more powerful closure than google::protobuf::Closure

namespace ananas {

namespace rpc {

class Closure : public ::google::protobuf::Closure {
public:
    // F must return void
    template <typename F, typename... Args,
              typename = typename std::enable_if<std::is_void<typename std::result_of<F (Args...)>::type>::value, void>::type>
    Closure(F&& f, Args&&... args) {
        func_ = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    }

    void Run() override final {
        func_();
        delete this; // Forgive the ugly raw pointer from protobuf
    }

private:
    std::function<void ()> func_;
};

} // end namespace rpc

} // end namespace ananas

#endif

