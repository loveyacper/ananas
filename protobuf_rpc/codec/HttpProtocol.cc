#include "HttpProtocol.h"

#include <errno.h>
#include <sys/uio.h>
#include <string.h>
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <string>

#define CRLF "\r\n"


// helpers

static
const char* Strstr(const char* ptr, size_t nBytes, const char* pattern, size_t nBytes2) {
    if (!pattern || *pattern == 0)
        return nullptr;

    const char* ret = std::search(ptr, ptr + nBytes, pattern, pattern + nBytes2);
    return ret == ptr + nBytes ? nullptr : ret;
}

static
const char* SearchCRLF(const char* ptr, size_t nBytes) {
    return Strstr(ptr, nBytes, CRLF, 2);
}

static
std::string& Trim(std::string &s) {
    if (s.empty()) {
        return s;
    }

    s.erase(0, s.find_first_not_of(" "));
    s.erase(s.find_last_not_of(" ") + 1);
    return s;
}



namespace ananas {

std::string HttpRequest::Encode() const {
    if (method_ == HttpMethod::Invalid)
        return std::string();

    std::string buf;
    buf.reserve(512);

    buf.append(MethodString());
    buf.append(" ");
    buf.append(path_);
    if (!query_.empty()) {
        buf.append("?");
        buf.append(query_);
    }
    buf.append(" HTTP/1.1" CRLF);

    for (const auto& kv : headers_) {
        buf.append(kv.first);
        buf.append(":");
        buf.append(kv.second);
        buf.append(CRLF);
    }

    buf.append(CRLF); // empty line before body
    if (!body_.empty())
        buf.append(body_);

    return buf;
}

std::vector<iovec> HttpRequest::EncodeLite() const {
    std::vector<iovec> ios;
    if (method_ == HttpMethod::Invalid)
        return ios;

    iovec iv;
    // request line
    const char* method = MethodString();
    iv.iov_base = const_cast<char*>(method);
    iv.iov_len = strlen(method);
    ios.push_back(iv);

    iv.iov_base = const_cast<char*>(" ");
    iv.iov_len = 1;
    ios.push_back(iv);

    iv.iov_base = const_cast<char*>(path_.data());
    iv.iov_len = path_.size();
    ios.push_back(iv);

    if (!query_.empty()) {
        iv.iov_base = const_cast<char*>("?");
        iv.iov_len = 1;
        ios.push_back(iv);

        iv.iov_base = const_cast<char*>(query_.data());
        iv.iov_len = query_.size();
        ios.push_back(iv);
    }

    iv.iov_base = const_cast<char*>(" HTTP/1.1\r\n");
    iv.iov_len = 11;
    ios.push_back(iv);

    for (const auto& kv : headers_) {
        iv.iov_base = const_cast<char*>(kv.first.data());
        iv.iov_len = kv.first.size();
        ios.push_back(iv);

        iv.iov_base = const_cast<char*>(":");
        iv.iov_len = 1;
        ios.push_back(iv);

        iv.iov_base = const_cast<char*>(kv.second.data());
        iv.iov_len = kv.second.size();
        ios.push_back(iv);

        iv.iov_base = const_cast<char*>(CRLF);
        iv.iov_len = 2;
        ios.push_back(iv);
    }

    iv.iov_base = const_cast<char*>(CRLF);
    iv.iov_len = 2;
    ios.push_back(iv); // empty line before body

    if (!body_.empty()) {
        iv.iov_base = const_cast<char*>(body_.data());
        iv.iov_len = body_.size();
        ios.push_back(iv);
    }

    return ios;
}


void HttpRequestParser::Reset() {
    waitMore_ = false;
    state_ = ParseState::RequestLine;
    contentLength_ = 0;
    request_.Reset();
}

bool HttpRequestParser::ParseRequest(const char*& ptr, const char* end) {
    waitMore_ = false;
    while (ptr <= end && !waitMore_ && !Done()) {
        switch (state_) {
            case ParseState::RequestLine:
                if (!_ParseRequestLine(ptr, end)) {
                    state_ = ParseState::Error;
                    return false;
                }
                break;

            case ParseState::Headers:
                if (!_ParseHeaders(ptr, end)) {
                    state_ = ParseState::Error;
                    return false;
                }
                break;

            case ParseState::Body:
                if (!_ParseBody(ptr, end)) {
                    state_ = ParseState::Error;
                    return false;
                }

                break;

            case ParseState::Done:
            default:
                break;
        }
    }

    return true;
}

const HttpRequest& HttpRequestParser::Request() const & {
    return request_;
}

bool HttpRequestParser::Done() const {
    return state_ == ParseState::Done;
}

bool HttpRequestParser::Error() const {
    return state_ == ParseState::Error;
}

bool HttpRequestParser::WaitMore() const {
    return waitMore_;
}

bool HttpRequestParser::_ParseRequestLine(const char*& ptr, const char* end) {
    assert (state_ == ParseState::RequestLine);
    const char* offset = ptr;
    const char* crlf = SearchCRLF(ptr, end - ptr);
    if (!crlf) {
        waitMore_ = true;
        return end - ptr < 1024; // Request line should not too long.
    }

    //GET / HTTP/1.1
    if (crlf - ptr < 14)
        return false;

    const char* space = std::find(offset, end, ' ');
    if (space == end) {
        //printf("Can't find method\n");
        return false;
    }

    if (!request_.SetMethod(std::string(offset, space))) {
        //printf("Invalid method %s\n", offset);
        return false;
    }

    // may be many spaces together
    offset = space;
    do {
        ++offset;
    } while (offset < end && *offset == ' ');

    if (offset == end) {
        return false;
    }

    space = std::find(offset, end, ' ');
    if (space == end) {
        //printf("Can't find uri\n");
        return false;
    }

    // check if has query
    const char* query = std::find(offset, space, '?');
    if (query != space) {
        request_.SetQuery(std::string(query+1, space));
    }

    request_.SetPath(std::string(offset, query));

    //ignore HTTP/version, default HTTP 1.1

    ptr = crlf + 2;
    state_ = ParseState::Headers;
    return true;
}

bool HttpRequestParser::_ParseHeaders(const char*& ptr, const char* end) {
    if (ptr == end) {
        const auto& len = request_.GetHeader("Content-Length");
        if (!len.empty()) {
            try {
                contentLength_ = size_t(std::stoi(len));
            } catch(...) {
                contentLength_ = 0;
            }
        }
        state_ = ParseState::Body;
        return true; // some error impl may ignore last CRLF before body
    }

    assert (state_ == ParseState::Headers);
    const char* crlf = SearchCRLF(ptr, end - ptr);
    if (!crlf) {
        waitMore_ = true;
        return (end - ptr < 1024); // single header k/v should not too big
    }

    if (crlf == ptr) {
        const auto& len = request_.GetHeader("Content-Length");
        if (!len.empty()) {
            try {
                contentLength_ = size_t(std::stoi(len));
            } catch(...) {
                contentLength_ = 0;
            }
        }
        state_ = ParseState::Body; // this is a empty line
        ptr = crlf + 2;
        return true;
    }

    if (crlf - ptr < 3)
        return false; // at least k:v

    const char* colon = std::find(ptr, crlf, ':');
    if (!colon) {
        //printf("No colon in header\n");
        return false;
    }

    auto field = std::string(ptr, colon);
    auto value = std::string(colon + 1, crlf);
    request_.SetHeader(Trim(field), Trim(value));

    ptr = crlf + 2;
    return true;
}

bool HttpRequestParser::_ParseBody(const char*& ptr, const char* end) {
    assert (state_ == ParseState::Body);
    if (request_.Body().size() >= contentLength_) {
        state_ = ParseState::Done;
        return true;
    }

    size_t needs = contentLength_ - request_.Body().size();

    if (ptr + needs > end) {
        needs = end - ptr;
        waitMore_ = true;
    }

    if (needs > 0) {
        const char* offset = ptr + needs;
        request_.AppendBody(ptr, needs);
        ptr = offset;
    }

    return true;
}



void HttpResponseParser::Reset() {
    waitMore_ = false;
    state_ = ParseState::StatusLine;
    contentLength_ = 0;
    response_.Reset();
}

bool HttpResponseParser::ParseResponse(const char*& ptr, const char* end) {
    waitMore_ = false;
    while (ptr <= end && !waitMore_ && !Done()) {
        switch (state_) {
            case ParseState::StatusLine:
                if (!_ParseStatusLine(ptr, end)) {
                    state_ = ParseState::Error;
                    return false;
                }
                break;

            case ParseState::Headers:
                if (!_ParseHeaders(ptr, end)) {
                    state_ = ParseState::Error;
                    return false;
                }
                break;

            case ParseState::Body:
                if (!_ParseBody(ptr, end)) {
                    state_ = ParseState::Error;
                    return false;
                }

                break;

            case ParseState::Done:
            default:
                break;
        }
    }

    return true;
}

const HttpResponse& HttpResponseParser::Response() const & {
    return response_;
}

bool HttpResponseParser::Done() const {
    return state_ == ParseState::Done;
}

bool HttpResponseParser::Error() const {
    return state_ == ParseState::Error;
}

bool HttpResponseParser::WaitMore() const {
    return waitMore_;
}

bool HttpResponseParser::_ParseStatusLine(const char*& ptr, const char* end) {
    assert (state_ == ParseState::StatusLine);
    const char* offset = ptr;
    const char* crlf = SearchCRLF(ptr, end - ptr);
    if (!crlf) {
        waitMore_ = true;
        return end - ptr < 1024; // Status line should not too long.
    }

    // HTTP/1.1 200 OK
    if (crlf - ptr < 10)
        return false;

    const char* space = std::find(offset, end, ' ');
    if (space == end) { // no version
        //printf("Can't find version\n");
        return false;
    }

    // may be many spaces together
    offset = space;
    do {
        ++offset;
    } while (offset < end && *offset == ' ');

    if (offset == end) {
        return false;
    }

    space = std::find(offset, end, ' ');
    if (space == end) {
        //printf("Can't find code\n");
        return false;
    }

    auto code = std::string(offset, space);
    try {
        int c = std::stoi(code);
        response_.SetCode(c);
    } catch(...) {
        return false;
    }

    // phase is optional
    // may be many spaces together
    offset = space;
    do {
        ++offset;
    } while (offset < end && *offset == ' ');

    if (offset < end) {
        response_.SetPhrase(std::string(offset, crlf));
    }

    ptr = crlf + 2;
    state_ = ParseState::Headers;
    return true;
}

bool HttpResponseParser::_ParseHeaders(const char*& ptr, const char* end) {
    assert (state_ == ParseState::Headers);
    const char* crlf = SearchCRLF(ptr, end - ptr);
    if (!crlf) {
        waitMore_ = true;
        return (end - ptr < 1024); // single header k/v should not too big
    }

    if (crlf == ptr) {
        const auto& len = response_.GetHeader("Content-Length");
        if (!len.empty()) {
            try {
                contentLength_ = size_t(std::stoi(len));
            } catch(...) {
                contentLength_ = 0;
            }
        }
        state_ = ParseState::Body; // this is a empty line
        ptr = crlf + 2;
        return true;
    }

    if (crlf - ptr < 3)
        return false; // at least k:v

    const char* colon = std::find(ptr, crlf, ':');
    if (!colon) {
        //printf("No colon in header\n");
        return false;
    }

    auto field = std::string(ptr, colon);
    auto value = std::string(colon + 1, crlf);
    response_.SetHeader(Trim(field), Trim(value));

    ptr = crlf + 2;
    return true;
}

bool HttpResponseParser::_ParseBody(const char*& ptr, const char* end) {
    assert (state_ == ParseState::Body);
    if (response_.Body().size() >= contentLength_) {
        state_ = ParseState::Done;
        return true;
    }

    size_t needs = contentLength_ - response_.Body().size();

    if (ptr + needs > end) {
        needs = end - ptr;
        waitMore_ = true;
    }

    if (needs > 0) {
        const char* offset = ptr + needs;
        response_.AppendBody(ptr, needs);
        ptr = offset;
    }

    return true;
}

std::string HttpResponse::Encode() const {
    std::string buf;
    buf.reserve(512);

    buf.append("HTTP/1.1 ");
    buf.append(std::to_string(code_));
    buf.append(" ");
    buf.append(phrase_);
    buf.append(CRLF);

    for (const auto& kv : headers_) {
        buf.append(kv.first);
        buf.append(":");
        buf.append(kv.second);
        buf.append(CRLF);
    }

    buf.append(CRLF); // empty line before body
    if (!body_.empty())
        buf.append(body_);

    return buf;
}


} // end namespace ananas


