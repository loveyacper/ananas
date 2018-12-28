#ifndef BERT_HTTPPROTOCOL_H
#define BERT_HTTPPROTOCOL_H

#include <cassert>
#include <unordered_map>
#include <string>

// toy http request parser, just for ananas rpc's health http service

namespace ananas {

namespace rpc {

enum class HttpMethod : int8_t
{
    Invalid,
    Get,
    Post,
    Head,
    Put,
    Delete
};

class HttpRequest {
public:
    void Reset() {
        method_ = HttpMethod::Invalid;
        path_.clear();
        //query_.clear();
        headers_.clear();
        body_.clear();
    }

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

    const std::string& Body() const {
        return body_;
    }

private:
    HttpMethod method_ { HttpMethod::Invalid };
    std::string path_;
    //std::string query_;
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
    HttpRequest request_;
};

} // end namespace rpc

} // end namespace ananas

#endif

