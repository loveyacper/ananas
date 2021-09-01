#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "RpcService.h"
#include "RpcClosure.h"
#include "RpcException.h"
#include "protobuf_rpc/ananas_rpc.pb.h"
#include "ananas/net/Connection.h"
#include "ananas/net/Application.h"
#include "ananas/net/AnanasDebug.h"
#include "ananas/util/Util.h"

namespace ananas {

namespace rpc {

Service::Service(GoogleService* service) :
    service_(service),
    name_(service->GetDescriptor()->full_name()) {
}

Service::~Service() {
}

GoogleService* Service::GetService() const {
    return service_.get();
}

void Service::SetEndpoint(const Endpoint& ep) {
    assert (!ep.ip().empty());
    endpoint_ = ep;
}

bool Service::Start() {
    if (endpoint_.ip().empty())
        return false;

    auto& app = Application::Instance();
    SocketAddr bindAddr(endpoint_.ip(), endpoint_.port());
    app.Listen(bindAddr, std::bind(&Service::OnNewConnection,
                                   this,
                                   std::placeholders::_1));
    return true;
}

const std::string& Service::FullName() const {
    return name_;
}

void Service::OnNewConnection(Connection* conn) {
    auto channel = std::make_shared<ServerChannel>(conn, this);
    conn->SetUserData(channel);
    conn->SetBatchSend(true);

    assert (conn->GetLoop()->Id() < static_cast<int>(channels_.size()));

    auto& channelMap = channels_[conn->GetLoop()->Id()];
    bool succ = channelMap.insert({conn->GetUniqueId(), channel.get()}).second;
    assert (succ);

    if (onCreateChannel_)
        onCreateChannel_(channel.get());

    conn->SetOnDisconnect(std::bind(&Service::_OnDisconnect, this, std::placeholders::_1));
    conn->SetOnMessage(&Service::_OnMessage);
}

void Service::OnRegister() {
    channels_.resize(Application::Instance().NumOfWorker());
}

void Service::SetMethodSelector(std::function<std::string (const Message* )> ms) {
    methodSelector_ = std::move(ms);
}

void Service::SetOnCreateChannel(std::function<void (ServerChannel* )> occ) {
    onCreateChannel_ = std::move(occ);
}

size_t Service::_OnMessage(Connection* conn, const char* data, size_t len) {
    const char* const start = data;
    size_t offset = 0;

    auto channel = conn->GetUserData<ServerChannel>();

    try {
        const char* const thisStart = data;
        auto msg = channel->OnData(data, len - offset);
        if (msg) {
            offset += (data - thisStart);
            try {
                channel->OnMessage(std::move(msg));
            } catch (const std::system_error& e) {
                auto code = e.code();
                assert (code.category() == AnanasCategory());
                channel->_OnError(e, code.value());
                switch (code.value()) {
                    case static_cast<int>(ErrorCode::NoSuchService):
                    case static_cast<int>(ErrorCode::NoSuchMethod):
                    case static_cast<int>(ErrorCode::EmptyRequest):
                    case static_cast<int>(ErrorCode::ThrowInMethod):
                        ANANAS_WRN << "RecovableException " << code.message();
                        return data - start;

                    case static_cast<int>(ErrorCode::DecodeFail):
                    case static_cast<int>(ErrorCode::MethodUndetermined):
                        ANANAS_ERR << "FatalException " << code.message();
                        conn->ActiveClose();
                        return data - start;

                    default:
                        ANANAS_ERR << "Unknown Exception " << code.message();
                        conn->ActiveClose();
                        return data - start;
                }
            } catch (...) {
                ANANAS_ERR << "OnMessage: Unknown error";
                conn->ActiveClose();
                return data - start;
            }
        }
    } catch (const std::system_error& e) {
        // Often because evil message
        ANANAS_ERR << "Some exception OnData " << e.what();
        conn->ActiveClose();
        return 0;
    } catch (const std::exception& e) {
        ANANAS_ERR << "Unknown exception OnData " << e.what();
        conn->ActiveClose();
        return 0;
    }

    return data - start;
}

void Service::_OnDisconnect(Connection* conn) {
    auto& channelMap = channels_[conn->GetLoop()->Id()];
    bool succ = channelMap.erase(conn->GetUniqueId());
    assert (succ);
}

ServerChannel::ServerChannel(ananas::Connection* conn, rpc::Service* service) :
    conn_(conn),
    service_(service),
    encoder_(PbToFrameResponseEncoder) {
}

ServerChannel::~ServerChannel() {
}

void ServerChannel::SetContext(std::shared_ptr<void> ctx) {
    ctx_ = std::move(ctx);
}

rpc::Service* ServerChannel::Service() const {
    return service_;
}

Connection* ServerChannel::GetConnection() const {
    return conn_;
}

void ServerChannel::SetEncoder(Encoder enc) {
    encoder_ = std::move(enc);
}

void ServerChannel::SetDecoder(Decoder dec) {
    decoder_ = std::move(dec);
}

std::shared_ptr<Message> ServerChannel::OnData(const char*& data, size_t len) {
    return decoder_.b2mDecoder_(data, len);
}

bool ServerChannel::OnMessage(std::shared_ptr<Message>&& req) {
    std::string method;
    RpcMessage* frame = dynamic_cast<RpcMessage*>(req.get());
    if (frame) {
        if (frame->has_request()) {
            currentId_ = frame->request().id();
            method = frame->request().method_name();

            if (frame->request().service_name() != service_->FullName())
                throw Exception(ErrorCode::NoSuchService,
                                frame->request().service_name() + \
                                " got, but expect [" + \
                                Service()->FullName() + "]");
        } else {
            throw Exception(ErrorCode::EmptyRequest,
                            "Service  [" + Service()->FullName() + \
                            "] expect request from " + GetConnection()->Peer().ToString());
        }
    } else {
        currentId_ = -1;
        if (!service_->methodSelector_) {
            ANANAS_ERR << "How to get method name from message? You forget to set methodSelector";
            throw Exception(ErrorCode::MethodUndetermined, "methodSelector not set for [" + service_->FullName() + "]");
        } else {
            method = service_->methodSelector_(req.get());
            //ANANAS_DBG << "Debug: get method [" << method.data() << "] from message";
        }
    }

    this->_Invoke(method, std::move(req));
    return true;
}

void ServerChannel::_Invoke(const std::string& methodName, std::shared_ptr<Message>&& req) {
    const auto googServ = service_->GetService();
    auto method = googServ->GetDescriptor()->FindMethodByName(methodName);
    if (!method) {
        ANANAS_ERR << "_Invoke: No such method " << methodName;
        throw Exception(ErrorCode::NoSuchMethod, "Not find method [" + methodName + "]");
    }

    if (decoder_.m2mDecoder_) {
        std::unique_ptr<Message> request(googServ->GetRequestPrototype(method).New());
        decoder_.m2mDecoder_(*req, *request);
        req.reset(request.release());
    }

    /*
     * The resource manage is a little dirty, because CallMethod accepts raw pointers.
     * Here is my solution:
     * 1. request must be delete when exit this function, so if you want to process request
     * async, you MUST copy it.
     * 2. response is managed by Closure, so use shared_ptr.
     * 3. Closure must be managed by raw pointer, so when call Closure::Run, it will execute
     * `delete this` when exit, at which time the response is also destroyed.
     */
    std::shared_ptr<Message> response(googServ->GetResponsePrototype(method).New());

    std::weak_ptr<ananas::Connection> wconn(std::static_pointer_cast<ananas::Connection>(conn_->shared_from_this()));
    auto done = new Closure(&ServerChannel::_OnServDone, this, wconn, currentId_, response);

    try {
        googServ->CallMethod(method, nullptr, req.get(), response.get(), done);
    } catch (const std::exception& e) {
        delete done;
        // U should never throw exception in rpc call!
        throw Exception(ErrorCode::ThrowInMethod, methodName + ", detail:" + e.what());
    }
}

void ServerChannel::_OnServDone(std::weak_ptr<ananas::Connection> wconn,
                                int id,
                                std::shared_ptr<Message> response) {
    auto conn = wconn.lock();
    if (!conn) return;

    RpcMessage frame;
    Response* rsp = frame.mutable_response();
    if (id >= 0) rsp->set_id(id);
    bool succ = encoder_.m2fEncoder_(response.get(), frame);
    assert (succ);

    // may be called from other thread, so use SafeSend
    if (encoder_.f2bEncoder_) {
        Buffer bytes = encoder_.f2bEncoder_(frame);
        conn->SafeSend(bytes.ReadAddr(), bytes.ReadableSize());
    } else {
        const auto& bytes = rsp->serialized_response();
        conn->SafeSend(bytes);
    }
}

void ServerChannel::_OnError(const std::exception& err, int code) {
    assert (conn_->GetLoop()->InThisLoop());

    RpcMessage frame;
    Response* rsp = frame.mutable_response();
    if (currentId_ != -1) rsp->set_id(currentId_);
    rsp->mutable_error()->set_msg(err.what());
    rsp->mutable_error()->set_errnum(code);

    bool succ = encoder_.m2fEncoder_(nullptr, frame);
    assert (succ);

    if (encoder_.f2bEncoder_) {
        Buffer bytes = encoder_.f2bEncoder_(frame);
        conn_->SendPacket(bytes);
    } else {
        const auto& bytes = rsp->serialized_response();
        conn_->SendPacket(bytes);
    }
}


} // end namespace rpc

} // end namespace ananas

