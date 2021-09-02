#ifndef BERT_HELPER_H
#define BERT_HELPER_H

/*
 * This file is modified from facebook folly, with my annotation
 */

/*
 * Copyright 2016 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <tuple>
#include <vector>
#include <memory>
#include <mutex>

namespace ananas {

template <typename T>
class Future;

template <typename T>
class Promise;

template <typename T>
class Try;

template <typename T>
struct TryWrapper;

namespace internal {

template<typename F, typename... Args>
using ResultOf = decltype(std::declval<F>()(std::declval<Args>()...));

// I don't know why, but must do it to cater compiler...
template <typename F, typename... Args>
struct ResultOfWrapper {
    using Type = ResultOf<F, Args...>;
};

// Test if F can be called with Args type
template<typename F, typename... Args>
struct CanCallWith {
    // SFINAE  Check
    template<typename T,
             typename Dummy = ResultOf<T, Args...>>
    static constexpr std::true_type
    Check(std::nullptr_t dummy) {
        return std::true_type{};
    };

    template<typename Dummy>
    static constexpr std::false_type
    Check(...) {
        return std::false_type{};
    };

    typedef decltype(Check<F>(nullptr)) type; // true_type if F can accept Args
    static constexpr bool value = type::value; // the integral_constant's value
};

template<typename F, typename... Args>
constexpr bool CanCallWith<F, Args...>::value;

// simple traits
template <typename T>
struct IsFuture : std::false_type {
    using Inner = T;
};

template <typename T>
struct IsFuture<Future<T>> : std::true_type {
    using Inner = T;
};

template<typename F, typename T>
struct CallableResult {
    // Test F call with arg type: void, T&&, T&, but do NOT choose Try type as args
    typedef
    typename std::conditional<
    CanCallWith<F>::value, // if true, F can call with void
                ResultOfWrapper<F>,
                typename std::conditional< // NO, F(void) is invalid
                CanCallWith<F, T&&>::value, // if true, F(T&&) is valid
                ResultOfWrapper<F, T&&>, // Yes, F(T&&) is ok
                ResultOfWrapper<F, T&> >::type>::type Arg;  // Resort to F(T&)

    // If ReturnsFuture::value is true, F returns another future type.
    typedef IsFuture<typename Arg::Type> IsReturnsFuture;

    // Future callback's result must be wrapped in another future
    typedef Future<typename IsReturnsFuture::Inner> ReturnFutureType;
};


// CallableResult specilization for void.
// I don't know why folly works without this...
template<typename F>
struct CallableResult<F, void> {
    // Test F call with arg type: void or Try(void)
    typedef
    typename std::conditional<
    CanCallWith<F>::value, // if true, F can call with void
                ResultOfWrapper<F>,
                typename std::conditional< // NO, F(void) is invalid
                CanCallWith<F, Try<void>&&>::value, // if true, F(Try<void>&&) is valid
                ResultOfWrapper<F, Try<void>&&>, // Yes, F(Try<void>&& ) is ok
                ResultOfWrapper<F, const Try<void>&>>::type>::type Arg;  // Above all failed, resort to F(const Try<void>&)

    // If ReturnsFuture::value is true, F returns another future type.
    typedef IsFuture<typename Arg::Type> IsReturnsFuture;

    // Future callback's result must be wrapped in another future
    typedef Future<typename IsReturnsFuture::Inner> ReturnFutureType;
};

//
// For when_all
//
template <typename... ELEM>
struct CollectAllVariadicContext {
    CollectAllVariadicContext() {}

    // Differ from folly: Do nothing here
    ~CollectAllVariadicContext() { }

    CollectAllVariadicContext(const CollectAllVariadicContext& ) = delete;
    void operator= (const CollectAllVariadicContext& ) = delete;

    template <typename T, size_t I>
    inline void SetPartialResult(typename TryWrapper<T>::Type&& t) {
        std::unique_lock<std::mutex> guard(mutex);

        std::get<I>(results) = std::move(t);
        collects.push_back(I);
        if (collects.size() == std::tuple_size<decltype(results)>::value) {
            guard.unlock();
            pm.SetValue(std::move(results));
        }
    }

    // Sorry, typedef does not work..
#define _TRYELEM_ typename TryWrapper<ELEM>::Type...
    Promise<std::tuple<_TRYELEM_>> pm;
    std::mutex mutex;
    std::tuple<_TRYELEM_> results;
    std::vector<size_t> collects;

    typedef Future<std::tuple<_TRYELEM_>>FutureType;
#undef _TRYELEM_
};

// base template
template <template <typename...> class CTX, typename... Ts>
void CollectVariadicHelper(const std::shared_ptr<CTX<Ts...>>& ) {
}

template <template <typename ...> class CTX, typename... Ts,
          typename THead, typename... TTail>
void CollectVariadicHelper(const std::shared_ptr<CTX<Ts...>>& ctx,
                           THead&& head, TTail&&... tail) {
    using InnerTry = typename TryWrapper<typename THead::InnerType>::Type;
    head.Then([ctx](InnerTry&& t) {
        ctx->template SetPartialResult<InnerTry,
                                       sizeof...(Ts) - sizeof...(TTail) - 1>(std::move(t));
    });

    CollectVariadicHelper(ctx, std::forward<TTail>(tail)...);
}


} // end namespace internal

} // end namespace ananas

#endif

