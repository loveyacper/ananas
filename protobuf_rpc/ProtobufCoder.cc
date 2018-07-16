#include "ProtobufCoder.h"
#include "ananas/util/Buffer.h"
#include "ananas_rpc.pb.h"

namespace ananas
{

namespace rpc
{
    
const int kPbHeaderLen = 4;

DecodeErrorException::DecodeErrorException(const char* w) :
    what_(w)
{
}

DecodeErrorException::DecodeErrorException(const std::string& w) :
    what_(w)
{
}
    
const char* DecodeErrorException::what() const noexcept
{
    return what_.c_str();
}

static int TotalPbLength(const char* data)
{
    int len;
    memcpy(&len, data, kPbHeaderLen); // no big endian

    return len;
}

std::shared_ptr<google::protobuf::Message> BytesToPbDecoder(const char*& data, size_t len)
{
    assert (len >= kPbHeaderLen);
    const int totalLen = TotalPbLength(data);
        
    // TODO totalLen limit
    if (totalLen <= kPbHeaderLen || totalLen >= 128 * 1024 * 1024)
        throw DecodeErrorException("abnormal totalLen " + std::to_string(totalLen));

    if (static_cast<int>(len) < totalLen)
        return nullptr;

    RpcMessage* frame =  nullptr;
    std::shared_ptr<google::protobuf::Message> res(frame = new RpcMessage);

    if (!frame->ParseFromArray(data + kPbHeaderLen, totalLen - kPbHeaderLen))
        throw DecodeErrorException("ParseFromArray failed");

    data += totalLen;
    return res;
}

DecodeState PbToMessageDecoder(const google::protobuf::Message& pbMsg, google::protobuf::Message& msg)
{
    const RpcMessage& frame = reinterpret_cast<const RpcMessage& >(pbMsg);
    if (frame.has_request())
    {
        msg.ParseFromString(frame.request().serialized_request());
    }
    else if (frame.has_response())
    {
        const auto& response = frame.response();
        if (HasField(response, "serialized_response"))
        {
            msg.ParseFromString(frame.response().serialized_response());
        }
        else
        {
            assert (HasField(response, "error")); 
            if (HasField(response.error(), "msg"))
                throw std::logic_error(response.error().msg());
            else
                throw std::logic_error("Errnum:" + std::to_string(response.error().errnum()));
        }
    }
    else
    {
        return DecodeState::eS_Error;
    }

    return DecodeState::eS_Ok;
}


bool PbToFrameRequestEncoder(const google::protobuf::Message* msg, RpcMessage& frame)
{
    Request* req = frame.mutable_request();
    if (msg)
        return msg->SerializeToString(req->mutable_serialized_request());
    else
        return true;
}

bool PbToFrameResponseEncoder(const google::protobuf::Message* msg, RpcMessage& frame)
{
    Response* rsp = frame.mutable_response();
    if (msg)
        return msg->SerializeToString(rsp->mutable_serialized_response());
    else
        return true;
}

ananas::Buffer PBFrameToBytesEncoder(const RpcMessage& rpcMsg)
{
    const int bodyLen = rpcMsg.ByteSize();
    const int totalLen = kPbHeaderLen + bodyLen;

    ananas::Buffer bytes;
    bytes.AssureSpace(sizeof totalLen + bodyLen);
    bytes.PushData(&totalLen, sizeof totalLen);

    bool succ = rpcMsg.SerializeToArray(bytes.WriteAddr(), bodyLen);
    if (!succ)
        bytes.Clear(); // TODO exception
    else
        bytes.Produce(bodyLen);

    return bytes;
}

bool HasField(const google::protobuf::Message& msg, const std::string& field)
{
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
    default_(true)
{
}

void Decoder::Clear()
{
    minLen_ = 0;
    BytesToMessageDecoder().swap(b2mDecoder_);
    MessageToMessageDecoder().swap(m2mDecoder_);
    default_ = false;
}

void Decoder::SetBytesToMessageDecoder(BytesToMessageDecoder b2m)
{
    if (default_)
        Clear();

    b2mDecoder_ = std::move(b2m);
}

void Decoder::SetMessageToMessageDecoder(MessageToMessageDecoder m2m)
{
    if (default_)
        Clear();

    m2mDecoder_ = std::move(m2m);
}

Encoder::Encoder(MessageToFrameEncoder enc) :
    m2fEncoder_(std::move(enc)),
    f2bEncoder_(PBFrameToBytesEncoder),
    default_(true)
{
}

void Encoder::Clear()
{
    default_ = false;
    MessageToFrameEncoder().swap(m2fEncoder_);
    FrameToBytesEncoder().swap(f2bEncoder_);
}

void Encoder::SetMessageToFrameEncoder(MessageToFrameEncoder m2f)
{
    if (default_)
        Clear();

    m2fEncoder_ = std::move(m2f);
}

void Encoder::SetFrameToBytesEncoder(FrameToBytesEncoder f2b)
{
    if (default_)
        Clear();

    f2bEncoder_ = std::move(f2b);
}


} // end namespace rpc

} // end namespace ananas

