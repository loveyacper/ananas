#include "ProtobufCoder.h"
#include "RpcException.h"
#include "ananas_rpc.pb.h"

using google::protobuf::Message;

namespace ananas {

namespace rpc {

const int kPbHeaderLen = 4;

static int TotalPbLength(const char* data) {
    int len;
    memcpy(&len, data, kPbHeaderLen); // no big endian

    return len;
}

std::shared_ptr<Message> BytesToPbDecoder(const char*& data, size_t len) {
    assert (len >= kPbHeaderLen);
    const int totalLen = TotalPbLength(data);

    // TODO totalLen limit
    if (totalLen <= kPbHeaderLen || totalLen >= 256 * 1024 * 1024)
        throw Exception(ErrorCode::TooLongFrame, "abnormal totalLen:" + std::to_string(totalLen));

    if (static_cast<int>(len) < totalLen)
        return nullptr;

    RpcMessage* frame =  nullptr;
    std::shared_ptr<Message> res(frame = new RpcMessage);

    if (!frame->ParseFromArray(data + kPbHeaderLen, totalLen - kPbHeaderLen))
        throw Exception(ErrorCode::DecodeFail, "ParseFromArray failed");

    data += totalLen;
    return res;
}

DecodeState PbToMessageDecoder(const Message& rpcMsg, Message& msg) {
    const RpcMessage& frame = dynamic_cast<const RpcMessage& >(rpcMsg);
    if (frame.has_request()) {
        msg.ParseFromString(frame.request().serialized_request());
    } else if (frame.has_response()) {
        const auto& response = frame.response();
        if (HasField(response, "serialized_response")) {
            msg.ParseFromString(response.serialized_response());
        } else if (HasField(response, "error")) {
            int err = HasField(response.error(), "errnum") ?  response.error().errnum() : 0;
            const auto msg = HasField(response.error(), "msg") ? response.error().msg() : "";
            throw Exception(static_cast<ErrorCode>(err), msg);
        } else {
            throw Exception(ErrorCode::DecodeFail, "EmptyResponse");
        }
    } else {
        throw Exception(ErrorCode::DecodeFail, "PbToMessageDecoder failed.");
    }

    return DecodeState::Ok;
}


bool PbToFrameRequestEncoder(const Message* msg, RpcMessage& frame) {
    Request* req = frame.mutable_request();
    if (msg)
        return msg->SerializeToString(req->mutable_serialized_request());
    else
        return true;
}

bool PbToFrameResponseEncoder(const Message* msg, RpcMessage& frame) {
    Response* rsp = frame.mutable_response();
    if (msg)
        return msg->SerializeToString(rsp->mutable_serialized_response());
    else
        return true;
}

Buffer PBFrameToBytesEncoder(const RpcMessage& rpcMsg) {
    const int bodyLen = rpcMsg.ByteSize();
    const int totalLen = kPbHeaderLen + bodyLen;

    Buffer bytes;
    bytes.AssureSpace(sizeof totalLen + bodyLen);
    bytes.PushData(&totalLen, sizeof totalLen);

    bool succ = rpcMsg.SerializeToArray(bytes.WriteAddr(), bodyLen);
    if (!succ) {
        bytes.Clear();
        throw Exception(ErrorCode::EncodeFail);
    } else {
        bytes.Produce(bodyLen);
    }

    return bytes;
}

bool HasField(const Message& msg, const std::string& field) {
    const auto descriptor = msg.GetDescriptor();
    assert(descriptor);

    const auto fieldDesc = descriptor->FindFieldByName(field);
    if (!fieldDesc)
        return false;

    const auto reflection = msg.GetReflection();
    return reflection->HasField(msg, fieldDesc);
}

Decoder::Decoder() :
    minLen_(kPbHeaderLen),
    b2mDecoder_(BytesToPbDecoder),
    m2mDecoder_(PbToMessageDecoder),
    default_(true) {
}

void Decoder::Clear() {
    minLen_ = 0;
    BytesToMessageDecoder().swap(b2mDecoder_);
    MessageToMessageDecoder().swap(m2mDecoder_);
    default_ = false;
}

void Decoder::SetBytesToMessageDecoder(BytesToMessageDecoder b2m) {
    if (default_)
        Clear();

    assert (!m2mDecoder_); // b2mDecoder_ must be set first!
    b2mDecoder_ = std::move(b2m);
}

void Decoder::SetMessageToMessageDecoder(MessageToMessageDecoder m2m) {
    assert (!default_);
    assert (b2mDecoder_); // b2mDecoder_ must be set first!

    assert (!m2mDecoder_);
    m2mDecoder_ = std::move(m2m);
}

Encoder::Encoder() :
    default_(false) {
}

Encoder::Encoder(MessageToFrameEncoder enc) :
    m2fEncoder_(std::move(enc)),
    f2bEncoder_(PBFrameToBytesEncoder),
    default_(true) {
}

void Encoder::Clear() {
    default_ = false;
    MessageToFrameEncoder().swap(m2fEncoder_);
    FrameToBytesEncoder().swap(f2bEncoder_);
}

void Encoder::SetMessageToFrameEncoder(MessageToFrameEncoder m2f) {
    if (default_)
        Clear();

    assert (!f2bEncoder_); // f2bEncoder_ must be set after m2fEncoder
    m2fEncoder_ = std::move(m2f);
}

void Encoder::SetFrameToBytesEncoder(FrameToBytesEncoder f2b) {
    assert (!default_); // if you set this encoder, message to frame must exist!
    assert (m2fEncoder_); // message to frame must exist!

    assert (!f2bEncoder_);
    f2bEncoder_ = std::move(f2b);
}


} // end namespace rpc

} // end namespace ananas

