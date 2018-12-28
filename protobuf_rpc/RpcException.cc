#include "RpcException.h"

namespace ananas {

namespace rpc {

AnanasErrorCategory::AnanasErrorCategory() {
}

const char* AnanasErrorCategory::name() const noexcept {
    return "ananas.error";
}

std::string AnanasErrorCategory::message(int ec) const {

    switch (static_cast<ErrorCode>(ec)) {
        case ErrorCode::None:
            return "ananas.error:OK";

        case ErrorCode::NoSuchService:
            return "ananas.error:NoSuchService";

        case ErrorCode::NoSuchMethod:
            return "ananas.error:NoSuchMethod";

        case ErrorCode::ConnectionLost:
            return "ananas.error:ConnectionLost";

        case ErrorCode::ConnectionReset:
            return "ananas.error:ConnectionReset";

        case ErrorCode::DecodeFail:
            return "ananas.error:DecodeFail";

        case ErrorCode::EncodeFail:
            return "ananas.error:EncodeFail";

        case ErrorCode::Timeout:
            return "ananas.error:Timeout";

        case ErrorCode::TooLongFrame:
            return "ananas.error:TooLongFrame";

        case ErrorCode::EmptyRequest:
            return "ananas.error:EmptyRequest";

        case ErrorCode::MethodUndetermined:
            return "ananas.error:MethodUndetermined";

        case ErrorCode::ThrowInMethod:
            return "ananas.error:ThrowInMethod";

        case ErrorCode::NoAvailableEndpoint:
            return "ananas.error:NoAvailableEndpoint";

        case ErrorCode::ConnectRefused:
            return "ananas.error:ConnectRefused";

        default:
            break;
    }

    return "Bad ananas.rpc error code";
}

const std::error_category& AnanasCategory() noexcept {
    static const AnanasErrorCategory cat;
    return cat;
}

std::system_error Exception(ErrorCode e, const std::string& msg) {
    return std::system_error(std::error_code(static_cast<int>(e),
                             AnanasCategory()),
                             msg);
}

} // end namespace rpc

} // end namespace ananas

