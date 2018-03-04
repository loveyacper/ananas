#include "ProtobufCoder.h"
#include "ananas_rpc.pb.h"

namespace ananas
{

namespace rpc
{
    
const int kPbHeaderLen = 4;

static int TotalPbLength(const char* data)
{
    int len;
    memcpy(&len, data, kPbHeaderLen); // no big endian

    return len;
}

DecodeState BytesToPBFrameDecoder(const char*& data, size_t len, RpcMessage& frame)
{
    assert (len >= kPbHeaderLen);

    const int totalLen = TotalPbLength(data);
    printf("totalLen = %d, real len = %d\n", totalLen, len);
    if (totalLen <= kPbHeaderLen || totalLen >= 100 * 1024 * 1024)
        return DecodeState::eS_Error;

    if (static_cast<int>(len) < totalLen)
        return DecodeState::eS_Waiting;

    if (!frame.ParseFromArray(data + kPbHeaderLen, totalLen - kPbHeaderLen))
        return DecodeState::eS_Error;

    data += totalLen;
    return DecodeState::eS_Ok;
}

bool PBFrameToMessageDecoder(const RpcMessage& frame, google::protobuf::Message* msg)
{
    if (frame.has_request())
        return msg->ParseFromString(frame.request().serialized_request());
    else if (frame.has_response())
        return msg->ParseFromString(frame.response().serialized_response());

    return false;
}

} // end namespace rpc

} // end namespace ananas

