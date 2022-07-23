#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "HttpParser.h"

namespace ananas {
class Connection;
class HttpContext;

class HttpServer {
 public:
  using Handler = std::function<std::unique_ptr<HttpResponse>(const HttpRequest&, std::shared_ptr<HttpContext>)>;
  using HandlerMap = std::unordered_map<std::string, Handler>;
  using OnNewClient = std::function<void(HttpContext*)>;

  const Handler& GetHandler(const std::string& url) const;
  void HandleFunc(const std::string& url, Handler handle);
  void SetOnNewHttpContext(OnNewClient on_new_http);

  void OnNewConnection(Connection* conn);

 private:
  void OnDisconnect(Connection* conn);

  std::unordered_map<int, std::shared_ptr<HttpContext>> contexts_;
  HandlerMap handlers_;

  OnNewClient on_new_http_ctx_;

  static Handler default_;
};

class HttpContext : public std::enable_shared_from_this<HttpContext> {
 public:
  HttpContext(llhttp_type type, std::shared_ptr<Connection> c, HttpServer* server);

  int Parse(Connection*, const char* data, int len);

  template <typename T>
  std::shared_ptr<T> GetUserData() const;
  void SetUserData(std::shared_ptr<void> data) { user_data_ = std::move(data); }

  void SendResponse(const HttpResponse& rsp);

 private:
  // entry for handle http request
  void HandleRequest(const HttpRequest& req);

  HttpParser parser_;
  std::weak_ptr<Connection> conn_;  // users may prolong life of ctx
  HttpServer* server_ = nullptr;

  std::shared_ptr<void> user_data_;
};

template <typename T>
inline std::shared_ptr<T> HttpContext::GetUserData() const {
  return std::static_pointer_cast<T>(user_data_);
}

}  // namespace ananas
