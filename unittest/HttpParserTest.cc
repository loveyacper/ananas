
#include <string.h>

#include "gtest/gtest.h"
#include "util/HttpProtocol.h"

using namespace ananas;
using namespace testing;

TEST(normal_request, simple_test) {
    HttpRequestParser parser;
    const char* req = "GET /bert HTTP/1.1\r\n"
        "Host : baidu.com\r\n"
        "\r\n";
    EXPECT_FALSE(parser.Done());
    parser.ParseRequest(req, req + strlen(req));
    EXPECT_TRUE(parser.Done());

    EXPECT_EQ(parser.Request().Method(), HttpMethod::Get);
    EXPECT_STREQ(parser.Request().Path().data(),"/bert");
    EXPECT_STREQ(parser.Request().GetHeader("Host").data(), "baidu.com");
    EXPECT_STREQ(parser.Request().Body().data(), "");
}

TEST(part_request, not_complete_req_test) {
    HttpRequestParser parser;
    const char* req = "GET /bert HTTP/1.1\r\n"
        "Host : baidu.com\r";

    EXPECT_FALSE(parser.Done());
    parser.ParseRequest(req, req + strlen(req));
    EXPECT_FALSE(parser.Done());
}

TEST(body_request, parse_body_test) {
    HttpRequestParser parser;
    const char* req = "GET /bert HTTP/1.1\r\n"
        "Host : baidu.com\r\n"
        "Content-Length : 10\r\n"
        "\r\n"
        "HelloWorld";

    EXPECT_FALSE(parser.Done());
    parser.ParseRequest(req, req + strlen(req));
    EXPECT_TRUE(parser.Done());

    EXPECT_EQ(parser.Request().Method(),HttpMethod::Get);
    EXPECT_STREQ(parser.Request().Path().data(),  "/bert");
    EXPECT_STREQ(parser.Request().GetHeader("Host").data(), "baidu.com");
    EXPECT_STREQ(parser.Request().Body().data(),"HelloWorld");
}

TEST(part_startline_request, not_comlete_startline_test) {
    HttpRequestParser parser;
    char data[1024] = "GET /bert HTTP/1.1";
    const char* req = data;

    EXPECT_FALSE(parser.Done());
    parser.ParseRequest(req, req + strlen(req));
    EXPECT_TRUE(req == data);
    EXPECT_FALSE(parser.Done());
    EXPECT_TRUE(parser.WaitMore());

    // continue parse: simulate recv more data from network.
    req = strcat(data, "\r\n Host : baidu.com\r\n"
        "Content-Length : 10\r\n"
        "\r\n"
        "HelloWorld");
    parser.ParseRequest(req, req + strlen(req));
    EXPECT_TRUE(parser.Done());

    EXPECT_TRUE(parser.Request().Method() == HttpMethod::Get);
    EXPECT_TRUE(parser.Request().Path() == "/bert");
    EXPECT_TRUE(parser.Request().GetHeader("Host") == "baidu.com");
    EXPECT_TRUE(parser.Request().Body() == "HelloWorld");
}

TEST(part_header_request, not_comlete_header_test) {
    HttpRequestParser parser;
    char data[1024] = "GET /bert HTTP/1.1\r\nHost:";
    const char* req = data;

    EXPECT_FALSE(parser.Done());
    parser.ParseRequest(req, req + strlen(req));
    EXPECT_TRUE(req != data);
    size_t offset = req - data;
    EXPECT_FALSE(parser.Done());
    EXPECT_TRUE(parser.WaitMore());

    // continue parse: simulate recv more data from network.
    req = strcat(data, " baidu.com\r\n"
        "Content-Length : 10\r\n"
        "\r\n"
        "HelloWorld");
    req += offset;
    parser.ParseRequest(req, req + strlen(req));
    EXPECT_TRUE(parser.Done());

    EXPECT_TRUE(parser.Request().Method() == HttpMethod::Get);
    EXPECT_TRUE(parser.Request().Path() == "/bert");
    EXPECT_TRUE(parser.Request().GetHeader("Host") == "baidu.com");
    EXPECT_TRUE(parser.Request().Body() == "HelloWorld");
}

TEST(part_body_request, not_comlete_body_test) {
    HttpRequestParser parser;
    const char* req = "GET /bert HTTP/1.1\r\n"
        "Host : baidu.com\r\n"
        "Content-Length : 10\r\n"
        "\r\n"
        "Hello";

    EXPECT_FALSE(parser.Done());
    parser.ParseRequest(req, req + strlen(req));
    EXPECT_FALSE(parser.Done());
    EXPECT_TRUE(*req == '\0');

    // continue parse
    req = "World";
    parser.ParseRequest(req, req + strlen(req));
    EXPECT_TRUE(parser.Done());

    EXPECT_TRUE(parser.Request().Method() == HttpMethod::Get);
    EXPECT_TRUE(parser.Request().Path() == "/bert");
    EXPECT_TRUE(parser.Request().GetHeader("Host") == "baidu.com");
    EXPECT_TRUE(parser.Request().Body() == "HelloWorld");
}

TEST(response, normal_response_test) {
    HttpResponseParser parser;
    const char* req = "HTTP/1.1 200 OK\r\n"
        "Host : baidu.com\r\n"
        "\r\n";
    EXPECT_FALSE(parser.Done());
    parser.ParseResponse(req, req + strlen(req));
    EXPECT_TRUE(parser.Done());

    EXPECT_TRUE(parser.Response().Code() == 200);
    EXPECT_TRUE(parser.Response().Phrase() == "OK");
    EXPECT_TRUE(parser.Response().GetHeader("Host") == "baidu.com");
    EXPECT_TRUE(parser.Response().Body() == "");
}

TEST(part_response, not_comlete_response_header_test) {
    HttpResponseParser parser;
    const char* req = "HTTP/1.1 200 OK\r\n"
        "Host:baidu.com\r";

    EXPECT_FALSE(parser.Done());
    parser.ParseResponse(req, req + strlen(req));
    EXPECT_FALSE(parser.Done());
}

TEST(body_response, parse_char_one_by_one) {
    HttpResponseParser parser;
    const char* req = "HTTP/1.1 200 OK\r\n"
        "ETag: \"abcdefg\"\r\n"
        "Date: 2017:11:16\r\n"
        "Content-Length : 10\r\n"
        "\r\n"
        "HelloWorld";
    const char* const raw = req;

    EXPECT_FALSE(parser.Done());
    for (size_t i = 0; i <= strlen(raw); ++i) {
        if (req < raw +i) {
            parser.ParseResponse(req, raw + i);
        }
    }

    EXPECT_TRUE(parser.Done());

    const auto& rsp = parser.Response();
    EXPECT_TRUE(rsp.Code() == 200);
    EXPECT_TRUE(rsp.Phrase() == "OK");
    EXPECT_TRUE(rsp.GetHeader("Date") == "2017:11:16");
    EXPECT_TRUE(rsp.GetHeader("ETag") == "\"abcdefg\"");
    EXPECT_TRUE(rsp.Body() == "HelloWorld");
}

TEST(part_body_response, two_phase_test) {
    HttpResponseParser parser;
    const char* req = "HTTP/1.1 200 OK\r\n"
        "Host : baidu.com\r\n"
        "Content-Length : 10\r\n"
        "\r\n"
        "Hello";

    EXPECT_FALSE(parser.Done());
    parser.ParseResponse(req, req + strlen(req));
    EXPECT_FALSE(parser.Done());
    EXPECT_TRUE(*req == '\0');

    // continue parse
    req = "World";
    parser.ParseResponse(req, req + strlen(req));
    EXPECT_TRUE(parser.Done());

    EXPECT_TRUE(parser.Response().Code() == 200);
    EXPECT_TRUE(parser.Response().Phrase() == "OK");
    EXPECT_TRUE(parser.Response().GetHeader("Host") == "baidu.com");
    EXPECT_TRUE(parser.Response().Body() == "HelloWorld");
}

