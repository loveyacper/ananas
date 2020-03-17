#ifndef BERT_PROTOBUFCODER_H
#define BERT_PROTOBUFCODER_H

#include <functional>
#include <memory>
#include <string>
#include <stdexcept>
#include "ananas/util/Buffer.h"

///@file ProtobufCoder.h
///@brief Default coders use protobuf
namespace google {
namespace protobuf {
class Message;
}
}

namespace ananas {

namespace rpc {

class RpcMessage;

enum class DecodeState {
    None,
    Waiting,
    Error,
    Ok,
};


extern const int kPbHeaderLen;

///@brief Bytes to Message Decoder type, it may throw exception.
///Use shared because Try<T>'s T must be copable
using BytesToMessageDecoder = std::function<std::shared_ptr<google::protobuf::Message> (const char*& data, size_t len)>;
std::shared_ptr<google::protobuf::Message> BytesToPbDecoder(const char*& data, size_t len);

///@brief Message to Message Decoder type, it may throw exception.
using MessageToMessageDecoder = std::function<DecodeState (const google::protobuf::Message&, google::protobuf::Message& )>;
DecodeState PbToMessageDecoder(const google::protobuf::Message&, google::protobuf::Message& );


///@brief Message to Frame Encoder type, return true if encode success.
using MessageToFrameEncoder = std::function<bool (const google::protobuf::Message*, RpcMessage& )>;
bool PbToFrameRequestEncoder(const google::protobuf::Message*, RpcMessage& );
bool PbToFrameResponseEncoder(const google::protobuf::Message*, RpcMessage& );

///@brief Frame to Bytes Encoder type, it may throw exception
using FrameToBytesEncoder = std::function<Buffer (const RpcMessage& )>;
Buffer PBFrameToBytesEncoder(const RpcMessage& );


// helper function
bool HasField(const google::protobuf::Message& msg, const std::string& field);

///@brief Decoder for inbound message.
///
/// b2bDecoder_ is optional, for ssl decrypt data or unzip data, etc.
/// b2mDecoder_ is required, bytes must be convert to message.
/// m2mDecoder_ is optional, for message convert.
/// For example, if receive https data, b2bDecoder_ is for SSLRead
/// to decrypt data, then pass to b2mDecoder_ for http parse.
/// m2mDecoder_ is useless in this case.
struct Decoder {
public:
    Decoder();
    void Clear();
    void SetBytesToMessageDecoder(BytesToMessageDecoder b2m);
    void SetMessageToMessageDecoder(MessageToMessageDecoder m2m);

    int minLen_;
    //BytesToBytesDecoder b2bDecoder_; // TODO for SSL decrypt, unzip etc
    BytesToMessageDecoder b2mDecoder_; ///< required
    MessageToMessageDecoder m2mDecoder_; ///< optional, for binary protocol
private:
    bool default_;
};

///@brief Encoder for outbound message.
///
/// m2fEncoder_ is required, bytes must be convert to frame.
/// f2bEncoder_ is optional, for binary protocol
/// b2bEncoder_ is optional, for ssl encrypt data or zip data, etc.
/// For example, if send https data, m2fEncoder_ is for http encode,
/// then pass to b2bEncoder_ for SSLWrite to encrypt.
/// f2bEncoder_ is useless in this case.
struct Encoder {
public:
    Encoder();
    Encoder(MessageToFrameEncoder enc);
    void Clear();
    void SetMessageToFrameEncoder(MessageToFrameEncoder m2f);
    void SetFrameToBytesEncoder(FrameToBytesEncoder f2b);

    MessageToFrameEncoder m2fEncoder_; ///< required
    FrameToBytesEncoder f2bEncoder_; ///< optional for binary protocol
    //BytesToBytesEncoder b2bEncoder_; // TODO for SSL encrypt, zip etc

private:
    bool default_;
};


} // end namespace rpc

} // end namespace ananas

#endif

