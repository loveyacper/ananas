#ifndef BERT_HELPER_H
#define BERT_HELPER_H

#include <tuple>
#include <vector>

template<typename F, typename... Args>
using ResultOf = decltype(std::declval<F>()(std::declval<Args>()...));  // F (Args...)

template <typename F, typename... Args>                                   
struct ResultOfWrapper
{
    using Type = ResultOf<F, Args...>;
};

template<typename F, typename... Args>        
struct CanCallWith {
    template<typename T,       
        // check if function T can accept Args as formal args.
        typename Dummy = ResultOf<T, Args...>>   // std::enable_if
        static constexpr std::true_type
        Check(std::nullptr_t dummy) {
            return std::true_type{};
        };  
                         
    template<typename Dummy>
        static constexpr std::false_type
        Check(...) {
            return std::false_type{};
        };
                                
    typedef decltype(Check<F>(nullptr)) type; // true_type if T可以接受Args作为参数        
    static constexpr bool value = type::value; // the integral_constant's value            
};              

template <typename T>
class Future;

template <typename T>
struct IsFuture : std::false_type { 
    using Inner = T;
    //using Inner = typename Unit::Lift<T>::type; 
};

template <typename T> 
struct IsFuture<Future<T>> : std::true_type { 
    typedef T Inner; 
};

template<typename F, typename T>
struct CallableResult { 
    // Test F call with arg type: void, T&&, T&, Do Not choose Try
    typedef
        typename std::conditional<
                 CanCallWith<F>::value, // if true, F can call with void
                 ResultOfWrapper<F>,
                 typename std::conditional< // NO, F(void) is invalid
                          CanCallWith<F, T&&>::value, // if true, F(T&&) is valid
                          ResultOfWrapper<F, T&&>, // Yes, F(T&&) is ok
                          ResultOfWrapper<F, T&> >::type>::type Arg;  // Resort to F(T&)
    typedef IsFuture<typename Arg::Type> IsReturnsFuture; // If ReturnsFuture::value is true, F returns another future type.
    typedef Future<typename IsReturnsFuture::Inner> ReturnFutureType; // 如果是返回另一个future，inner是future的内嵌类型。如果不是，原样。
};

#if 1
// TODO 这个可能有问题 I don't know why folly works, but I must specilize for void...
template<typename F>
struct CallableResult<F, void> { 
    // Test F call with arg type: void
    typedef ResultOfWrapper<F> Arg;
#if 0
    typedef
        typename std::conditional<
                 CanCallWith<F>::value, // if true, F can call with void
                 ResultOfWrapper<F>,
                 void>::type Arg;  // Resort to F(T&)
#endif
    typedef IsFuture<typename Arg::Type> IsReturnsFuture; // If ReturnsFuture::value is true, F returns another future type.
    typedef Future<typename IsReturnsFuture::Inner> ReturnFutureType; // 如果是返回另一个future，inner是future的内嵌类型。如果不是，原样。
};
#endif

template <typename T>
class Promise;
template <typename T>
class Future;
template <typename T>
class Try;

#if 1
template <typename... ELEM> 
struct CollectAllVariadicContext {
    CollectAllVariadicContext() {}
    CollectAllVariadicContext(const CollectAllVariadicContext& ) = delete;
    void operator= (const CollectAllVariadicContext& ) = delete;
    
    template <typename T, size_t I>
    inline void SetPartialResult(Try<T>& t) {
        std::get<I>(results) = std::move(t);
        collects.push_back(I);
        if (collects.size() == std::tuple_size<decltype(results)>::value) {
            std::cout << "SetValue All!!! \n";
                
            pm.SetValue(std::move(results));
        }
    }
        
    ~CollectAllVariadicContext() {
    }
    
    Promise<std::tuple<Try<ELEM>...>> pm;
    std::tuple<Try<ELEM>...> results;
    std::vector<size_t> collects;
    
    typedef Future<std::tuple<Try<ELEM>...>> FutureType;
};

template <template <typename...> class CTX, typename... Ts>
void CollectVariadicHelper(const std::shared_ptr<CTX<Ts...>>& )
{
}
      
template <template <typename ...> class CTX, typename... Ts,
         typename THead, typename... TTail> 
void CollectVariadicHelper(const std::shared_ptr<CTX<Ts...>>& ctx, 
     THead&& head, TTail&&... tail)
{
    head.SetCallback([ctx](Try<typename THead::InnerType>&& t) { 
         ctx->template SetPartialResult<typename THead::InnerType, 
         sizeof...(Ts) - sizeof...(TTail) - 1>(t); 
         }); 
 // template tail-recursion 
 CollectVariadicHelper(ctx, std::forward<TTail>(tail)...); 
}

#else
#endif

#endif

