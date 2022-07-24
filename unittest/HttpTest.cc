#include <chrono>
#include <cstdio>
#include <future>
#include <memory>
#include <thread>

#include "net/Application.h"
#include "gtest/gtest.h"
#include "net/http/HttpParser.h"
#include "net/http/HttpServer.h"

using namespace ananas;

static const int kPort = 51327;
static const char* const kUrl = "/v1/play";

static std::unique_ptr<HttpResponse> ExampleHttpHandler(const HttpRequest& req, std::shared_ptr<HttpContext>) {
  std::unique_ptr<HttpResponse> rsp(new HttpResponse);
  rsp->SetCode(200);
  rsp->SetStatus("status test");
  rsp->SetHeader("Content-Length", "0");

  return rsp;
}

class HttpTest : public testing::Test {
 public:
  HttpTest() = default;
  ~HttpTest() = default;

  static void SetUpTestCase() {
    app_ = &Application::Instance();
    loop_ = app_->BaseLoop();
    thd_ = std::thread([]() {
      HttpTest::server_ = HttpTest::app_->ListenHTTP("127.0.0.1", kPort);
      HttpTest::server_->HandleFunc(kUrl, ExampleHttpHandler);

      HttpTest::server2_ = HttpTest::app_->ListenHTTP("127.0.0.1", kPort + 1);

      HttpTest::app_->Run(0, nullptr);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    printf("SetUpTestCase\n");
  }

  static void TearDownTestCase() {
    printf("TearDownTestCase\n");
    app_->Exit();

    // must wait thd_ exit.
    thd_.join();
    app_->Reset();
  }

  static Application* app_;
  static EventLoop* loop_;
  static std::thread thd_;
  static std::shared_ptr<HttpServer> server_;
  static std::shared_ptr<HttpServer> server2_;
};

Application* HttpTest::app_{nullptr};
EventLoop* HttpTest::loop_{nullptr};
std::thread HttpTest::thd_;
std::shared_ptr<HttpServer> HttpTest::server_;
std::shared_ptr<HttpServer> HttpTest::server2_;

TEST_F(HttpTest, Handle404) {
  HttpRequest req;
  req.SetMethod(HTTP_GET);
  req.SetUrl("/not-exist");

  std::promise<void> done;
  app_->ConnectHTTP("127.0.0.1", kPort)->SendRequest(req, [&done](const HttpResponse& rsp) {
    EXPECT_EQ(rsp.Code(), 404);
    done.set_value();
  });

  done.get_future().get();
}

TEST_F(HttpTest, HandleTimeout) {
  const std::string url("/timeout");
  HttpTest::server_->HandleFunc(
      url, [url](const HttpRequest& req, std::shared_ptr<HttpContext>) -> std::unique_ptr<HttpResponse> {
        EXPECT_EQ(req.Url(), url);
        EXPECT_EQ(req.Method(), HTTP_GET);

        // let client timeout
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        std::unique_ptr<HttpResponse> rsp(new HttpResponse);
        rsp->SetHeader("Content-Length", "0");
        return rsp;
      });

  HttpRequest req;
  req.SetMethod(HTTP_GET);
  req.SetUrl(url);

  std::promise<void> timeout;
  auto client = app_->ConnectHTTP("127.0.0.1", kPort);
  client->SetTimeout(1);
  client->SendRequest(
      req, [](const HttpResponse& rsp) { assert(0); },
      [&timeout](const HttpRequest& req, HttpClient::ErrorCode err) {
        EXPECT_TRUE(err == HttpClient::ErrorCode::kTimeout);
        timeout.set_value();
      });

  timeout.get_future().get();
  // wait later response
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

TEST_F(HttpTest, HandleEcho) {
  std::string path("/echo");
  std::string content_type("text/plain");
  std::string content("hello world");

  HttpTest::server_->HandleFunc(
      path, [&](const HttpRequest& req, std::shared_ptr<HttpContext>) -> std::unique_ptr<HttpResponse> {
        EXPECT_EQ(req.Url(), path);
        EXPECT_EQ(req.Method(), HTTP_POST);
        EXPECT_EQ(req.Body(), content);

        std::unique_ptr<HttpResponse> rsp(new HttpResponse);
        rsp->SetCode(200);
        rsp->SetBody(req.Body());
        return rsp;
      });

  auto check = [&](const HttpResponse& rsp) {
    EXPECT_EQ(rsp.Code(), 200);
    EXPECT_EQ(rsp.Body(), content);
  };

  HttpRequest request1;
  HttpRequest request2;

  request1.SetMethod(HTTP_POST);
  request1.SetBody(content);
  request1.SetUrl(path);
  request1.SetHeader("Content-Type", content_type);
  request2 = request1;

  auto client = app_->ConnectHTTP("127.0.0.1", kPort);
  std::promise<void> done;
  client->SendRequest(request1, [client, &request2, &done, &check](const HttpResponse& rsp) {
    check(rsp);
    bool ok = client->SendRequest(request2, [&done, &check](const HttpResponse& rsp) {
      check(rsp);
      done.set_value();
    });
    EXPECT_TRUE(ok);
  });

  done.get_future().get();
}

TEST_F(HttpTest, RelayEcho) {
  llhttp_method_t method = HTTP_POST;
  std::string path("/echo");
  std::string content_type("text/plain");
  std::string content("hello world");

  auto server2_handler = [&](const HttpRequest& req, std::shared_ptr<HttpContext>) {
    EXPECT_EQ(req.Url(), path);
    EXPECT_EQ(req.Method(), method);
    EXPECT_EQ(req.Body(), content);

    std::unique_ptr<HttpResponse> rsp(new HttpResponse);
    rsp->SetCode(200);
    rsp->SetBody(req.Body());
    return rsp;
  };
  server2_->HandleFunc(path, server2_handler);

  auto server_handler = [&](const HttpRequest& client_req, std::shared_ptr<HttpContext> ctx) {
    // relay request in server side
    app_->ConnectHTTP("127.0.0.1", kPort + 1)->SendRequest(client_req, [&content, ctx](const HttpResponse& rsp) {
      EXPECT_EQ(rsp.Body(), content);

      HttpResponse rsp2;
      rsp2.SetCode(200);
      rsp2.SetStatus("status test");
      rsp2.SetBody(content);

      ctx->SendResponse(rsp2);
    });
    return nullptr;  // no response until relay back
  };
  server_->HandleFunc(path, server_handler);

  HttpRequest client_req;
  client_req.SetMethod(HTTP_POST);
  client_req.SetUrl(path);
  client_req.SetBody(content);
  client_req.SetHeader("Content-Type", content_type);

  auto client = app_->ConnectHTTP("127.0.0.1", kPort);

  std::promise<void> done;
  client->SendRequest(client_req, [&](const HttpResponse& rsp) {
    EXPECT_EQ(rsp.Code(), 200);
    EXPECT_EQ(rsp.Body(), content);

    done.set_value();
  });

  done.get_future().get();  // wait request done
}

TEST_F(HttpTest, NotPipeline) {
  const std::string url("/url");
  HttpTest::server_->HandleFunc(
      url, [url](const HttpRequest& req, std::shared_ptr<HttpContext>) -> std::unique_ptr<HttpResponse> {
        EXPECT_EQ(req.Url(), url);
        EXPECT_EQ(req.Method(), HTTP_GET);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        std::unique_ptr<HttpResponse> rsp(new HttpResponse);
        rsp->SetCode(200);
        rsp->SetBody(req.Body());
        return rsp;
      });

  HttpRequest req;
  req.SetMethod(HTTP_GET);
  req.SetUrl(url);

  std::promise<void> done;
  auto client = app_->ConnectHTTP("127.0.0.1", kPort);
  bool ok = client->SendRequest(req, [&](const HttpResponse& rsp) { done.set_value(); });
  EXPECT_TRUE(ok);
  ok = client->SendRequest(req, [](const HttpResponse& rsp) {});
  EXPECT_FALSE(ok);

  done.get_future().get();
}
