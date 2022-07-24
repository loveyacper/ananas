
#include "net/http/HttpParser.h"

#include <string.h>

#include "gtest/gtest.h"

using namespace ananas;
using namespace testing;

class HttpRequestParser : public Test {
 public:
  HttpRequestParser() : parser_(HTTP_REQUEST) {}
  HttpParser parser_;
};

class HttpResponseParser : public Test {
 public:
  HttpResponseParser() : parser_(HTTP_RESPONSE) {}
  HttpParser parser_;
};

#define CRLF "\r\n"

// ------->> Begin : case from llhttp
TEST_F(HttpRequestParser, llhttp_simple) {
  std::string req("OPTIONS /url HTTP/1.1" CRLF "Header1: Value1" CRLF "Header2:\t Value2" CRLF CRLF);

  EXPECT_TRUE(parser_.Execute(req));
  EXPECT_TRUE(parser_.IsComplete());

  // tab/space ignored
  EXPECT_EQ(parser_.Request().GetHeader("Header1"), "Value1");
  EXPECT_EQ(parser_.Request().GetHeader("Header2"), "Value2");

  EXPECT_EQ(parser_.Request().Url(), "/url");
  EXPECT_EQ(parser_.Request().Method(), HTTP_OPTIONS);
}

TEST_F(HttpRequestParser, llhttp_curl_request) {
  std::string req("GET /test HTTP/1.1" CRLF
                  "User-Agent: curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 "
                  "OpenSSL/0.9.8g zlib/1.2.3.3 libidn/1.1" CRLF "Host: 0.0.0.0=5000" CRLF "Accept: */*" CRLF CRLF);

  EXPECT_TRUE(parser_.Execute(req));
  EXPECT_TRUE(parser_.IsComplete());

  // tab/space ignored
  EXPECT_EQ(parser_.Request().GetHeader("User-Agent"),
            "curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 OpenSSL/0.9.8g "
            "zlib/1.2.3.3 libidn/1.1");
  EXPECT_EQ(parser_.Request().GetHeader("Host"), "0.0.0.0=5000");
  EXPECT_EQ(parser_.Request().GetHeader("Accept"), "*/*");

  EXPECT_EQ(parser_.Request().Url(), "/test");
  EXPECT_EQ(parser_.Request().Method(), HTTP_GET);
}

TEST_F(HttpRequestParser, llhttp_prefix_newline_request) {
  std::string req(CRLF "GET /test HTTP/1.1" CRLF CRLF);

  EXPECT_TRUE(parser_.Execute(req));
  EXPECT_TRUE(parser_.IsComplete());

  EXPECT_TRUE(parser_.Request().Headers().empty());

  EXPECT_EQ(parser_.Request().Url(), "/test");
  EXPECT_EQ(parser_.Request().Method(), HTTP_GET);
}

TEST_F(HttpRequestParser, llhttp_query) {
  std::string req("GET http://hypnotoad.org?hail=all HTTP/1.1" CRLF CRLF);

  EXPECT_TRUE(parser_.Execute(req));
  EXPECT_TRUE(parser_.IsComplete());

  EXPECT_TRUE(parser_.Request().Headers().empty());

  EXPECT_EQ(parser_.Request().Url(), "http://hypnotoad.org?hail=all");
  EXPECT_EQ(parser_.Request().Method(), HTTP_GET);
}

TEST_F(HttpRequestParser, llhttp_space_in_uri) {
  std::string req("GET /foo bar/ HTTP/1.1" CRLF CRLF);

  EXPECT_FALSE(parser_.Execute(req));
  EXPECT_FALSE(parser_.IsComplete());
  EXPECT_TRUE(parser_.ErrorReason().size() > 0);
}

TEST_F(HttpRequestParser, put_with_content_len) {
  const char* req = "PUT /url HTTP/1.1" CRLF "Content-Length: 003" CRLF "Ohai: world" CRLF CRLF "abc";

  EXPECT_TRUE(parser_.Execute(req, strlen(req)));
  EXPECT_TRUE(parser_.IsComplete());

  EXPECT_EQ(parser_.Request().Body(), "abc");
  EXPECT_EQ(parser_.Request().ContentLength(), 3);
  EXPECT_EQ(parser_.Request().GetHeader("Ohai"), "world");
}
// <<------- End : case from llhttp

TEST_F(HttpRequestParser, simple_request) {
  const char* req =
      "GET /bert HTTP/1.1\r\n"
      "Host : baidu.com\r\n"
      "\r\n";

  EXPECT_FALSE(parser_.IsComplete());
  EXPECT_TRUE(parser_.Execute(req, strlen(req)));
  EXPECT_TRUE(parser_.IsComplete());

  EXPECT_EQ(parser_.Request().Method(), HTTP_GET);
  EXPECT_EQ(parser_.Request().Url(), "/bert");
  EXPECT_EQ(parser_.Request().GetHeader("Host"), "baidu.com");
  EXPECT_TRUE(parser_.Request().Body().empty());
}

TEST_F(HttpRequestParser, not_complete_request) {
  const char* req =
      "GET /bert HTTP/1.1\r\n"
      "Host : baidu.com\r";

  EXPECT_FALSE(parser_.IsComplete());
  EXPECT_TRUE(parser_.Execute(req, strlen(req)));
  EXPECT_FALSE(parser_.IsComplete());
}

TEST_F(HttpRequestParser, parse_request_body) {
  const char* req =
      "GET /bert HTTP/1.1\r\n"
      "Host : baidu.com\r\n"
      "Content-Length : 10\r\n"
      "\r\n"
      "HelloWorld";

  EXPECT_FALSE(parser_.IsComplete());
  EXPECT_TRUE(parser_.Execute(req, strlen(req)));
  EXPECT_TRUE(parser_.IsComplete());

  EXPECT_EQ(parser_.Request().Method(), HTTP_GET);
  EXPECT_EQ(parser_.Request().Url(), "/bert");
  EXPECT_EQ(parser_.Request().GetHeader("Host"), "baidu.com");
  EXPECT_EQ(parser_.Request().Body(), "HelloWorld");
}

TEST_F(HttpRequestParser, not_complete_startline) {
  const std::string start_half("GET /bert HTTP/1.1");

  EXPECT_FALSE(parser_.IsComplete());
  EXPECT_TRUE(parser_.Execute(start_half));
  EXPECT_FALSE(parser_.IsComplete());

  // continue parse: simulate recv more data from network.
  const std::string end_half(
      "\r\n Host : baidu.com\r\n"
      "Content-Length : 10\r\n"
      "\r\n"
      "HelloWorld");
  EXPECT_TRUE(parser_.Execute(end_half));
  EXPECT_TRUE(parser_.IsComplete());

  EXPECT_EQ(parser_.Request().Method(), HTTP_GET);
  EXPECT_EQ(parser_.Request().Url(), "/bert");
  EXPECT_EQ(parser_.Request().GetHeader("Host"), "baidu.com");
  EXPECT_EQ(parser_.Request().Body(), "HelloWorld");
}

TEST_F(HttpRequestParser, not_comlete_header) {
  const std::string start_half("GET /bert HTTP/1.1\r\nHost:");

  EXPECT_FALSE(parser_.IsComplete());
  EXPECT_TRUE(parser_.Execute(start_half));
  EXPECT_FALSE(parser_.IsComplete());

  // continue parse: simulate recv more data from network.
  const std::string end_half(
      " baidu.com\r\n"
      "Content-Length : 10\r\n"
      "\r\n"
      "HelloWorld");
  EXPECT_TRUE(parser_.Execute(end_half));
  EXPECT_TRUE(parser_.IsComplete());

  EXPECT_EQ(parser_.Request().Method(), HTTP_GET);
  EXPECT_EQ(parser_.Request().Url(), "/bert");
  EXPECT_EQ(parser_.Request().GetHeader("Host"), "baidu.com");
  EXPECT_EQ(parser_.Request().Body(), "HelloWorld");
}

TEST_F(HttpRequestParser, not_comlete_body) {
  const std::string start_half(
      "GET /bert HTTP/1.1\r\n"
      "Host : baidu.com\r\n"
      "Content-Length : 10\r\n"
      "\r\n"
      "Hello");

  EXPECT_FALSE(parser_.IsComplete());
  EXPECT_TRUE(parser_.Execute(start_half));
  EXPECT_FALSE(parser_.IsComplete());

  // continue parse
  EXPECT_TRUE(parser_.Execute("World", 5));
  EXPECT_TRUE(parser_.IsComplete());

  EXPECT_EQ(parser_.Request().Method(), HTTP_GET);
  EXPECT_EQ(parser_.Request().Url(), "/bert");
  EXPECT_EQ(parser_.Request().GetHeader("Host"), "baidu.com");
  EXPECT_EQ(parser_.Request().Body(), "HelloWorld");
}

TEST_F(HttpRequestParser, repeat_parse_requests) {
  const char* req =
      "GET /bert HTTP/1.1\r\n"
      "Host : baidu.com\r\n"
      "\r\n";

  for (int i = 0; i < 3; i++) {
    EXPECT_TRUE(parser_.Execute(req, strlen(req)));
    EXPECT_TRUE(parser_.IsComplete());

    EXPECT_EQ(parser_.Request().Url(), "/bert");
    EXPECT_EQ(parser_.Request().GetHeader("Host"), "baidu.com");
  }
}

TEST_F(HttpRequestParser, reinit_after_fail) {
  const char* bad =
      "GET HTTP/1.1\r\n"
      "\r\n";
  EXPECT_FALSE(parser_.Execute(bad, strlen(bad)));
  EXPECT_FALSE(parser_.IsComplete());
  parser_.Reinit();  // recover from failed state

  const char* good =
      "GET /bert HTTP/1.1\r\n"
      "Host : baidu.com\r\n"
      "\r\n";

  EXPECT_TRUE(parser_.Execute(good, strlen(good)));
  EXPECT_TRUE(parser_.IsComplete());

  EXPECT_EQ(parser_.Request().Url(), "/bert");
  EXPECT_EQ(parser_.Request().GetHeader("Host"), "baidu.com");
}

TEST_F(HttpRequestParser, parse_more_than_one_with_handler) {
  const char* more_than_one =
      "GET /bert HTTP/1.1\r\n"
      "\r\n"
      "GET /xyz HTTP/1.1\r\n"
      "Host : xxx.com";

  int got_req = 0;
  std::string url;
  parser_.SetRequestHandler([&](const HttpRequest& req) {
    got_req++;
    url = req.Url();
  });

  EXPECT_TRUE(parser_.Execute(more_than_one, strlen(more_than_one)));
  EXPECT_FALSE(parser_.IsComplete());
  EXPECT_EQ(got_req, 1);
  EXPECT_EQ(url, "/bert");
}

TEST_F(HttpResponseParser, normal_response) {
  const char* req =
      "HTTP/1.1 200 OK\r\n"
      "Host : baidu.com\r\n"
      "Content-Length : 0\r\n"
      "\r\n";

  EXPECT_FALSE(parser_.IsComplete());
  EXPECT_TRUE(parser_.Execute(req, strlen(req)));
  EXPECT_TRUE(parser_.IsComplete());

  EXPECT_EQ(parser_.Response().Code(), 200);
  EXPECT_EQ(parser_.Response().Status(), "OK");
  EXPECT_EQ(parser_.Response().GetHeader("Host"), "baidu.com");
  EXPECT_TRUE(parser_.Response().Body().empty());
}

TEST_F(HttpResponseParser, not_comlete_response_header) {
  const char* req =
      "HTTP/1.1 200 OK\r\n"
      "Host:baidu.com\r";

  EXPECT_FALSE(parser_.IsComplete());
  EXPECT_TRUE(parser_.Execute(req, strlen(req)));
  EXPECT_FALSE(parser_.IsComplete());
}

TEST_F(HttpResponseParser, parse_char_one_by_one) {
  const char* req =
      "HTTP/1.1 200 OK\r\n"
      "ETag: \"abcdefg\"\r\n"
      "Date: 2017:11:16\r\n"
      "Content-Length : 10\r\n"
      "\r\n"
      "HelloWorld";

  for (size_t i = 0; i < strlen(req); ++i) {
    EXPECT_TRUE(parser_.Execute(&req[i], 1));
  }

  EXPECT_TRUE(parser_.IsComplete());

  const auto& rsp = parser_.Response();
  EXPECT_EQ(rsp.Code(), 200);
  EXPECT_EQ(rsp.Status(), "OK");
  EXPECT_EQ(rsp.GetHeader("Date"), "2017:11:16");
  EXPECT_EQ(rsp.GetHeader("ETag"), "\"abcdefg\"");
  EXPECT_EQ(rsp.Body(), "HelloWorld");
}

TEST_F(HttpResponseParser, not_complete_body) {
  const char* req =
      "HTTP/1.1 200 OK\r\n"
      "Host : baidu.com\r\n"
      "Content-Length : 10\r\n"
      "\r\n"
      "Hello";

  EXPECT_TRUE(parser_.Execute(req, strlen(req)));
  EXPECT_FALSE(parser_.IsComplete());

  // continue parse
  req = "World";
  EXPECT_TRUE(parser_.Execute(req, strlen(req)));
  EXPECT_TRUE(parser_.IsComplete());

  EXPECT_EQ(parser_.Response().Code(), 200);
  EXPECT_EQ(parser_.Response().Status(), "OK");
  EXPECT_EQ(parser_.Response().GetHeader("Host"), "baidu.com");
  EXPECT_EQ(parser_.Response().Body(), "HelloWorld");
}

TEST_F(HttpResponseParser, reinit_after_fail) {
  const char* rsp =
      "HTTP/1.1 abc OK\r\n"
      "Content-Length : 0\r\n"
      "\r\n";

  EXPECT_FALSE(parser_.Execute(rsp, strlen(rsp)));
  EXPECT_FALSE(parser_.IsComplete());

  // continue parse after reinit
  parser_.Reinit();
  rsp =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length : 0\r\n"
      "\r\n";
  EXPECT_TRUE(parser_.Execute(rsp, strlen(rsp)));
  EXPECT_TRUE(parser_.IsComplete());

  EXPECT_EQ(parser_.Response().Code(), 200);
  EXPECT_EQ(parser_.Response().Status(), "OK");
}

// parse url
TEST(UrlParser, url_test) {
  std::string url("/cloud/reconnect?stream_id=xxx&sdp=yyy");
  auto result = ParseUrl(url);
  EXPECT_EQ(result.path, "/cloud/reconnect");
  EXPECT_EQ(result.query, "stream_id=xxx&sdp=yyy");

  std::string stream_id = ParseQuery(url, "stream_id");
  EXPECT_EQ(stream_id, "xxx");
  std::string sdp = ParseQuery(url, "sdp");
  EXPECT_EQ(sdp, "yyy");
  std::string empty = ParseQuery(url, "none");
  EXPECT_EQ(empty, "");

  url = "/cloud/reconnect";
  result = ParseUrl(url);
  EXPECT_EQ(result.path, "/cloud/reconnect");
  EXPECT_EQ(result.query, "");

  url = "/cloud/reconnect?";
  result = ParseUrl(url);
  EXPECT_EQ(result.path, "/cloud/reconnect");
  EXPECT_EQ(result.query, "");

  url = "";
  result = ParseUrl(url);
  EXPECT_EQ(result.path, "");
  EXPECT_EQ(result.query, "");
}
