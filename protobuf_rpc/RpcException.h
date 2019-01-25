#ifndef BERT_RPCEXCEPTION_H
#define BERT_RPCEXCEPTION_H

#include <string>
#include <system_error>

///@file RpcException.h
namespace ananas {

namespace rpc {

///@brief ErrorCode for rpc exception
enum class ErrorCode {
    None,

    // both
    NoSuchService, ///< Service name may be wrong.
    NoSuchMethod, ///< Method name may be wrong.
    ConnectionLost, ///< Connection lost, you can retry rpc::Call
    ConnectionReset, ///< Connection reset, you can retry rpc::Call
    DecodeFail, ///< Decode message failed, may be evil message.
    EncodeFail, ///< Encode message failed, please check.
    Timeout, ///< Timeout rpc call
    TooLongFrame, ///< Too big message(64MB+), please check.

    // server-side
    EmptyRequest, ///< Request field is missing.
    MethodUndetermined, ///< Can't know call which method, need call [SetMethodSelector](@ref Service::SetMethodSelector)
    ThrowInMethod, ///< Server's method implementation throw some exception, but exception can't propagate via rpc.

    // client-side
    NoAvailableEndpoint, ///< All server dead
    ConnectRefused, ///< Server is not listen, try again.
};

class AnanasErrorCategory : public std::error_category {
public:
    AnanasErrorCategory();

    AnanasErrorCategory(const AnanasErrorCategory& ) = delete;
    AnanasErrorCategory& operator=(const AnanasErrorCategory& ) = delete;

    const char* name() const noexcept override;
    std::string message(int ec) const override;
};

// For ananas internal error handle
const std::error_category& AnanasCategory() noexcept;
std::system_error Exception(ErrorCode e, const std::string& msg = "");

} // end namespace rpc

} // end namespace ananas

#endif

