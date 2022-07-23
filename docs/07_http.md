# http server

```cpp
auto http_server = Application::Instance().ListenHTTP("0.0.0.0", 80);
http_server->HandleFunc("/v1/xxx", GetInfoHandler);

std::unique_ptr<HttpResponse> GetInfoHandler(const HttpRequest& req, std::shared_ptr<HttpContext> client_ctx) {
  // TODO xxx
  return std::make_unique<HttpResponse>(nullptr);
}
```

# http client

```cpp
HttpRequest req;
req.SetMethod(HTTP_POST);
req.SetUrl("/v1/xxx");
req.SetHeader("Host", ip);
req.SetHeader("Content-Type", "application/json");
req.SetBody("json data");

auto client = Application::Instance().ConnectHTTP("127.0.0.1", 80);
client->SetTimeout(10); // 10ms timeout
client->SendRequest(req, HandleRequest, HandleError);

void HandleRequest(const HttpResponse& rsp) {
  // TODO xxx
}

void HandleError(std::shared_ptr<HttpContext> client, HttpClient::ErrorCode err) {
  switch (err) {
    case HttpClient::ErrorCode::kConnectFail:
      cout << "connect failed";
      break;

    case HttpClient::ErrorCode::kConnectionReset:
      cout << "connection reset";
      break;

    case HttpClient::ErrorCode::kTimeout:
      cout << "request timeout";
      break;

    default:
      break;
  }
}
```
