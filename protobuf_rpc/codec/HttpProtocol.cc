#include "HttpProtocol.h"

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

namespace ananas {

namespace rpc {
    
void HttpRequestParser::Reset() {
    waitMore_ = false;
    state_ = ParseState::RequestLine;
    request_.Reset();
}

bool HttpRequestParser::ParseRequest(const char*& ptr, const char* end) {
    waitMore_ = false;
    while (!waitMore_ && !Done() && ptr <= end) {
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
        return end - ptr < 1024; // RequestLine should not too big
    }

    if (crlf - ptr < 12)
        return false;

    const char* space = std::find(offset, end, ' ');
    if (space == end) // no method
        return false;

    if (!request_.SetMethod(std::string(offset, space)))
        return false;

    offset = space + 1;

    space = std::find(offset, end, ' ');
    if (space == end) // no path
        return false;
    request_.SetPath(std::string(offset, space));
    offset = space + 1;

    // ignore HTTP/version

    ptr = crlf + 2;
    state_ = ParseState::Headers;
    return true;
}

bool HttpRequestParser::_ParseHeaders(const char*& ptr, const char* end) {
    if (ptr == end) {
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
        state_ = ParseState::Body; // this is a empty line
        ptr = crlf + 2;
        return true;
    }

    if (crlf - ptr < 3)
        return false; // at least k:v

    const char* colon = std::find(ptr, crlf, ':');
    if (!colon)
        return false;

    request_.SetHeader(std::string(ptr, colon), std::string(colon + 1, crlf));

    ptr = crlf + 2;
    return true;
}

bool HttpRequestParser::_ParseBody(const char*& ptr, const char* end) {
    state_ = ParseState::Done;
    return true;
}

} // end namespace rpc

} // end namespace ananas

