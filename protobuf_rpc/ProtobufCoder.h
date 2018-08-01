#ifndef BERT_PROTOBUFCODER_H
#define BERT_PROTOBUFCODER_H

#include <functional>
#include <memory>
#include <string>
#include <stdexcept>

namespace google {
namespace protobuf {
class Message;
}
}


namespace ananas {

class Buffer;

namespace rpc {

class RpcMessage;

enum class DecodeState {
    eS_None,
    eS_Waiting,
    eS_Error,
    eS_Ok,
};


extern const int kPbHeaderLen;

class DecodeErrorException : public std::exception {
public:
    DecodeErrorException(const char* w);
    DecodeErrorException(const std::string& w);
    const char* what() const noexcept override;
private:
    std::string what_;
};

// decoder
// use shared because Try<T>'s T must be copable
using BytesToMessageDecoder = std::function<std::shared_ptr<google::protobuf::Message> (const char*& data, size_t len)>;
std::shared_ptr<google::protobuf::Message> BytesToPbDecoder(const char*& data, size_t len);

using MessageToMessageDecoder = std::function<DecodeState (const google::protobuf::Message&, google::protobuf::Message& )>;
DecodeState PbToMessageDecoder(const google::protobuf::Message&, google::protobuf::Message& );


// encoder
using MessageToFrameEncoder = std::function<bool (const google::protobuf::Message*, RpcMessage& )>;
bool PbToFrameRequestEncoder(const google::protobuf::Message*, RpcMessage& );
bool PbToFrameResponseEncoder(const google::protobuf::Message*, RpcMessage& );

using FrameToBytesEncoder = std::function<ananas::Buffer (const RpcMessage& )>;
ananas::Buffer PBFrameToBytesEncoder(const RpcMessage& );


// helper function
bool HasField(const google::protobuf::Message& msg, const std::string& field);

struct Decoder {
public:
    Decoder();
    void Clear();
    void SetBytesToMessageDecoder(BytesToMessageDecoder b2m);
    void SetMessageToMessageDecoder(MessageToMessageDecoder m2m);

    int minLen_;
    //BytesToBytesDecoder b2bDecoder_; // TODO for SSL
    BytesToMessageDecoder b2mDecoder_;
    MessageToMessageDecoder m2mDecoder_;
private:
    bool default_;
};

struct Encoder {
public:
    Encoder(MessageToFrameEncoder enc);
    void Clear();
    void SetMessageToFrameEncoder(MessageToFrameEncoder m2f);
    void SetFrameToBytesEncoder(FrameToBytesEncoder f2b);

    MessageToFrameEncoder m2fEncoder_;
    FrameToBytesEncoder f2bEncoder_;
    //BytesToBytesEncoder b2bEncoder_; // TODO for SSL

private:
    bool default_;
};


} // end namespace rpc

} // end namespace ananas

#endif

