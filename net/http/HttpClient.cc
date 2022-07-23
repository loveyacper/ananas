#include "HttpClient.h"

#include <assert.h>

#include "net/EventLoop.h"
#include "net/Connection.h"

namespace ananas {
HttpClient::HttpClient() : parser_(HTTP_RESPONSE) {}

void HttpClient::OnConnect(Connection* conn) {
  assert(loop_ == conn->GetLoop());

  //INFO("HttpClient::OnConnect to {}:{} in loop {}", conn->GetPeerIp(), conn->GetPeerPort(), loop_->GetName());
  never_connected_ = false;

  conn_ = std::static_pointer_cast<Connection>(conn->shared_from_this());
  parser_.SetResponseHandler(std::bind(&HttpClient::HandleResponse, this, std::placeholders::_1));

  conn->SetUserData(shared_from_this());
  conn->SetOnMessage(
      std::bind(&HttpClient::Parse, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  conn->SetOnDisconnect(std::bind(&HttpClient::OnDisconnect, this, std::placeholders::_1));

  FlushBufferedRequest();
}

void HttpClient::OnConnectFail(const char* peer_ip, int port) {
  assert(never_connected_);

  //WARN("HttpClient::OnConnectFail to {}:{} in loop {}", peer_ip, port, loop_->GetName());
  if (buffered_request_) {
    const auto& req = *buffered_request_;
    if (req.error_handle) {
      req.error_handle(req.request, ErrorCode::kConnectFail);
    }

   // WARN("HttpClient::OnConnectFail with request {}", req.request.Encode());
    buffered_request_.reset();
  }
}

bool HttpClient::SendRequest(const HttpRequest& req, HttpResponseHandler handle, ErrorHandler err_handle) {
  assert(loop_);

  if (loop_->InThisLoop()) {
    return DirectSendRequest(req, handle, err_handle);
  }

  loop_->Execute([this, req, handle, err_handle]() { this->DirectSendRequest(req, handle, err_handle); });
  return true;
}

void HttpClient::SetLoop(EventLoop* loop) { loop_ = loop; }

int HttpClient::Parse(Connection*, const char* data, int len) {
  bool ok = parser_.Execute(data, len);
  if (!ok) {
    //ERROR("failed parse http rsp: {}", data);
    return -1;
  }

  return len;
}

void HttpClient::HandleResponse(const HttpResponse& rsp) {
  if (!pending_request_) {
    //WARN("http resp {} after timeout: {}", rsp.Encode(), timeout_ms_);
    return;
  }

  auto ctx = std::move(pending_request_);
  ctx->handle(rsp);
}

bool HttpClient::DirectSendRequest(const HttpRequest& req, HttpResponseHandler handle, ErrorHandler err_handle) {
  assert(loop_->InThisLoop());

  auto build_ctx = [&]() -> std::shared_ptr<RequestContext> {
    auto ctx = std::make_shared<RequestContext>();
    ctx->request = req;
    ctx->handle = std::move(handle);
    ctx->error_handle = std::move(err_handle);

    MaySetTimeout(ctx);
    return ctx;
  };

  auto conn = conn_.lock();
  if (!conn) {
    if (!never_connected_) {
      //WARN("HttpClient conn was reset");
      if (err_handle) {
        err_handle(req, ErrorCode::kConnectionReset);
      }
      return false;
    }

    // never connected
    if (buffered_request_) {
      if (err_handle) {
        err_handle(req, ErrorCode::kNoPipeline);
      }
      return false;  // not support pipeline
    }

    buffered_request_ = build_ctx();
    return true;
  }

  if (pending_request_) {
    if (err_handle) {
      err_handle(req, ErrorCode::kNoPipeline);
    }
    return false;  // not support pipeline
  }

  auto req_bytes = req.Encode();
  if (!conn->SendPacket(req_bytes.data(), req_bytes.size())) {
    if (err_handle) {
      err_handle(req, ErrorCode::kConnectionReset);
    }
    return false;
  }

  pending_request_ = build_ctx();
  return true;
}

void HttpClient::MaySetTimeout(const std::shared_ptr<RequestContext>& ctx) {
  if (!ctx->error_handle || timeout_ms_ <= 0) {
    return;
  }

  // set timeout handler
  std::weak_ptr<HttpClient> w_cli(shared_from_this());
  std::weak_ptr<RequestContext> w_ctx(ctx);
  loop_->ScheduleLater(std::chrono::milliseconds(timeout_ms_), [w_cli, w_ctx]() {
    auto cli = w_cli.lock();
    if (!cli) {
      return;  // connection already lost
    }

    auto ctx = w_ctx.lock();
    if (!ctx) {
      return;  // already has response
    }

    ctx->error_handle(ctx->request, ErrorCode::kTimeout);

    // clear context
    cli->buffered_request_.reset();
    cli->pending_request_.reset();
  });
}

void HttpClient::OnDisconnect(Connection*) {
  if (pending_request_) {
    auto req = std::move(pending_request_);
    if (req->error_handle) {
      req->error_handle(req->request, ErrorCode::kConnectionReset);
    }

    //WARN("HttpClient::OnDisconnect pending request {}", req->request.Encode());
  }
}

void HttpClient::FlushBufferedRequest() {
  assert(!never_connected_);

  if (buffered_request_) {
    auto req = std::move(buffered_request_);
    SendRequest(req->request, req->handle, req->error_handle);
    //INFO("HttpClient send buffered request {}", req->request.Encode());
  }
}

}  // namespace ananas
