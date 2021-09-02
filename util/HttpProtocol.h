#ifndef BERT_HTTPPROTOCOL_H
#define BERT_HTTPPROTOCOL_H

#include <cassert>
#include <unordered_map>
#include <string>
#include <vector>

struct iovec;
namespace ananas {

enum class HttpMethod : int8_t {
    Invalid,
    Get,
    Post,
    Head,
    Put,
    Delete
};

// HTTP Request
class HttpRequest {
public:
    void Reset() {
        method_ = HttpMethod::Invalid;
        path_.clear();
        query_.clear();
        headers_.clear();
        body_.clear();
    }

    // Encode to string buffer, deep copy
    std::string Encode() const;
    // Shallow copy
    std::vector<iovec> EncodeLite() const;

    bool SetMethod(const std::string& m) {
        assert(method_ == HttpMethod::Invalid);

        if (m == "GET")
            method_ = HttpMethod::Get;
        else if (m == "POST")
            method_ = HttpMethod::Post;
        else if (m == "HEAD")
            method_ = HttpMethod::Head;
        else if (m == "PUT")
            method_ = HttpMethod::Put;
        else if (m == "DELETE")
            method_ = HttpMethod::Delete;
        else
            return false;

        return true;
    }

    HttpMethod Method() const {
        return method_;
    }

    const char* MethodString() const {
        switch (method_) {
            case HttpMethod::Get:
                return "GET";
            case HttpMethod::Post:
                return "POST";
            case HttpMethod::Head:
                return "HEAD";
            case HttpMethod::Put:
                return "PUT";
            case HttpMethod::Delete:
                return "DELETE";
            default:
                return "Invalid method";
        }
    }

    void SetPath(const std::string& path) {
        path_ = path;
    }

    const std::string& Path() const {
        return path_;
    }

    void SetQuery(const std::string& q) {
        query_ = q;
    }

    const std::string& Query() const {
        return query_;
    }

    void SetHeader(const std::string& field, const std::string& value) {
        headers_.insert({field, value});
    }

    const std::string& GetHeader(const std::string& field) const {
        static const std::string kEmpty;
        auto it = headers_.find(field);
        return it == headers_.end() ? kEmpty : it->second;
    }

    const std::unordered_map<std::string, std::string>& Headers() const {
        return headers_;
    }

    void SetBody(const std::string& body) {
        body_ = body;
    }

    void SetBody(const char* body, size_t len) {
        body_.assign(body, len);
    }


    void AppendBody(const std::string& body) {
        body_ += body;
    }

    void AppendBody(const char* body, size_t len) {
        body_.append(body, len);
    }

    const std::string& Body() const {
        return body_;
    }

    void Swap(HttpRequest& other) {
        std::swap(method_, other.method_);
        path_.swap(other.path_);
        query_.swap(other.query_);
        headers_.swap(other.headers_);
        body_.swap(other.body_);
    }

private:
    HttpMethod method_ { HttpMethod::Invalid };
    std::string path_;
    std::string query_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};


class HttpRequestParser {
public:
    void Reset();
    bool ParseRequest(const char*& ptr, const char* end);

    const HttpRequest& Request() const &;
    bool Done() const;
    bool Error() const;
    bool WaitMore() const;

private:
    bool _ParseRequestLine(const char*& ptr, const char* end);
    bool _ParseHeaders(const char*& ptr, const char* end);
    bool _ParseBody(const char*& ptr, const char* end);

    enum class ParseState {
        Error,
        RequestLine,
        Headers,
        Body,
        Done,
    };

    bool waitMore_ = false;
    ParseState state_ = ParseState::RequestLine;
    size_t contentLength_ {0};
    HttpRequest request_;
};

enum HttpCode {
    OK = 200,
    Created = 201,
    Accepted = 202,
    NonAuthInfo = 203,
    NoContent = 204,

    BadRequest = 400,
    Unauth = 401,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllow = 405,

    InnerServerError = 500,
    NotImpl = 501,
    BadGateway = 502,
    ServiceUnavail = 503,
};

// HTTP Response
class HttpResponse {
public:
    void Reset() {
        code_ = 200;
        phrase_.clear();
        headers_.clear();
        body_.clear();
    }

    void SetCode(int code) {
        code_ = code;
    }

    int Code() const {
        return code_;
    }

    void SetPhrase(const std::string& phrase) {
        phrase_ = phrase;
    }

    const std::string& Phrase() const {
        return phrase_;
    }

    void SetHeader(const std::string& field, const std::string& value) {
        headers_.insert({field, value});
    }

    const std::string& GetHeader(const std::string& field) const {
        static const std::string kEmpty;
        auto it = headers_.find(field);
        return it == headers_.end() ? kEmpty : it->second;
    }

    const std::unordered_map<std::string, std::string>& Headers() const {
        return headers_;
    }

    void SetBody(const std::string& body) {
        body_ = body;
    }

    void AppendBody(const std::string& body) {
        body_ += body;
    }

    void AppendBody(const char* body, size_t len) {
        body_.append(body, len);
    }

    const std::string& Body() const {
        return body_;
    }

    // Encode to string buffer
    std::string Encode() const;

private:
    int code_{200};
    std::string phrase_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};


class HttpResponseParser {
public:
    void Reset();
    bool ParseResponse(const char*& ptr, const char* end);

    const HttpResponse& Response() const &;
    bool Done() const;
    bool Error() const;
    bool WaitMore() const;

private:
    bool _ParseStatusLine(const char*& ptr, const char* end);
    bool _ParseHeaders(const char*& ptr, const char* end);
    bool _ParseBody(const char*& ptr, const char* end);

    enum class ParseState {
        Error,
        StatusLine,
        Headers,
        Body,
        Done,
    };

    bool waitMore_ = false;
    ParseState state_ = ParseState::StatusLine;
    size_t contentLength_ {0};
    HttpResponse response_;
};

} // end namespace ananas

#endif

