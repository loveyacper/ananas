#ifndef BERT_RPCEXCEPTION_H
#define BERT_RPCEXCEPTION_H

#include <string>
#include <system_error>

namespace ananas {

namespace rpc {

enum class ErrorCode {
    None,   

    // both
    NoSuchService,
    NoSuchMethod,
    ConnectionLost,
    ConnectionReset,
    DecodeFail,
    EncodeFail,
    Timeout,

    // server-side
    EmptyRequest,
    MethodUndetermined,
    ThrowInMethod,

    // client-side
    NoAvailableEndpoint,
    ConnectRefused,
};

class AnanasErrorCategory : public std::error_category {
public:
    constexpr AnanasErrorCategory();

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

