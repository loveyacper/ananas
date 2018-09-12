#include "RpcException.h"

namespace ananas {

namespace rpc {

constexpr AnanasErrorCategory::AnanasErrorCategory() {
}

const char* AnanasErrorCategory::name() const noexcept {
    return "ananas.rpc.error";
}

std::string AnanasErrorCategory::message(int ec) const {

    switch (static_cast<ErrorCode>(ec)) {
        case ErrorCode::None:
            return "ananas.rpc.error:OK";

        case ErrorCode::NoSuchService:
            return "ananas.rpc.error:NoSuchService";

        case ErrorCode::NoSuchMethod:
            return "ananas.rpc.error:NoSuchMethod";

        case ErrorCode::ConnectionLost:
            return "ananas.rpc.error:ConnectionLost";

        case ErrorCode::ConnectionReset:
            return "ananas.rpc.error:ConnectionReset";

        case ErrorCode::DecodeFail:
            return "ananas.rpc.error:DecodeFail";

        case ErrorCode::EncodeFail:
            return "ananas.rpc.error:EncodeFail";

        case ErrorCode::Timeout:
            return "ananas.rpc.error:Timeout";

        case ErrorCode::EmptyRequest:
            return "ananas.rpc.error:EmptyRequest";

        case ErrorCode::MethodUndetermined:
            return "ananas.rpc.error:MethodUndetermined";

        case ErrorCode::ThrowInMethod:
            return "ananas.rpc.error:ThrowInMethod";

        case ErrorCode::NoAvailableEndpoint:
            return "ananas.rpc.error:NoAvailableEndpoint";

        case ErrorCode::ConnectRefused:
            return "ananas.rpc.error:ConnectRefused";

        default:
            break;
    }
            
    return "bad ananas.rpc error code";
}

const std::error_category& AnanasCategory() noexcept {
    static const AnanasErrorCategory cat;
    return cat;
}

std::system_error Exception(ErrorCode e, const std::string& msg) {
    return std::system_error(std::error_code(static_cast<int>(e), AnanasCategory()), msg);
}

} // end namespace rpc

} // end namespace ananas

