#include <functional>

namespace google
{
namespace protobuf
{
class Message;
}
}


namespace ananas
{

namespace rpc
{

class RpcMessage;

enum class DecodeState
{
    eS_None,
    eS_Waiting,
    eS_Error,
    eS_Ok,
};


extern const int kPbHeaderLen;
    
using BytesToFrameDecoder = std::function<DecodeState (const char*& data, size_t len, RpcMessage& )>;
DecodeState BytesToPBFrameDecoder(const char*& data, size_t len, RpcMessage& );

using FrameToMessageDecoder = std::function<bool (const RpcMessage& frame, google::protobuf::Message* msg)>;
bool PBFrameToMessageDecoder(const RpcMessage& frame, google::protobuf::Message* msg);

} // end namespace rpc

} // end namespace ananas

