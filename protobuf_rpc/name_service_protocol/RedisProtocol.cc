
#include <cassert>
#include <algorithm>
#include "RedisProtocol.h"

// 1 request -> multi strlist
// 2 multi -> * number crlf
// 3 strlist -> str strlist | empty
// 4 str -> strlen strval
// 5 strlen -> $ number crlf
// 6 strval -> string crlf

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
ParseResult GetIntUntilCRLF(const char*& ptr, std::size_t nBytes, int& val) {
    if (nBytes < 3)
        return ParseResult::wait;

    std::size_t i = 0;
    bool negtive = false;
    if (ptr[0] == '-') {
        negtive = true;
        ++ i;
    } else if (ptr[0] == '+') {
        ++ i;
    }

    int value = 0;
    for (; i < nBytes; ++ i) {
        if (isdigit(ptr[i])) {
            value *= 10;
            value += ptr[i] - '0';
        } else {
            if (ptr[i] != '\r' || (i+1 < nBytes && ptr[i+1] != '\n'))
                return ParseResult::error;

            if (i + 1 == nBytes)
                return ParseResult::wait;

            break;
        }
    }

    if (negtive)
        value *= -1;

    ptr += i;
    ptr += 2;
    val = value;
    return ParseResult::ok;
}


void ServerProtocol::Reset() {
    multi_ = -1;
    paramLen_ = -1;
    params_.clear();
    content_.clear();
    nParams_ = 0;
}

ParseResult ServerProtocol::ParseRequest(const char*& ptr, const char* end) {
    if (multi_ == -1) {
        const char* const start = ptr;
        auto parseRet = _ParseMulti(ptr, end, multi_);
        if (parseRet == ParseResult::error ||
                multi_ < -1)
            return ParseResult::error;

        if (parseRet != ParseResult::ok)
            return ParseResult::wait;

        content_.append(start, ptr);
        params_.emplace_back(start, ptr - start);
    }

    return _ParseStrlist(ptr, end, params_);
}

ParseResult ServerProtocol::_ParseMulti(const char*& ptr, const char* end, int& result) {
    if (end - ptr < 3)
        return ParseResult::wait;

    if (*ptr != '*')
        return ParseResult::error;

    ++ ptr;

    return GetIntUntilCRLF(ptr,  end - ptr, result);
}

ParseResult ServerProtocol::_ParseStrlist(const char*& ptr, const char* end, std::vector<std::string>& results) {
    while (nParams_ < multi_) {
        std::string res;
        auto parseRet = _ParseStr(ptr, end, res);

        if (parseRet == ParseResult::ok) {
            results.emplace_back(std::move(res));
            ++ nParams_;
        } else {
            return parseRet;
        }
    }

    return ParseResult::ok;
}

ParseResult ServerProtocol::_ParseStr(const char*& ptr, const char* end, std::string& res) {
    if (paramLen_ == -1) {
        const char* const start = ptr;
        auto parseRet = _ParseStrlen(ptr, end, paramLen_);
        if (parseRet == ParseResult::error ||
                paramLen_ < -1)
            return ParseResult::error;

        if (parseRet != ParseResult::ok)
            return ParseResult::wait;

        content_.append(start, ptr);
        params_.emplace_back(start, ptr - start);
    }

    if (paramLen_ == -1) {
        res.clear();
        return ParseResult::ok;
    } else {
        return _ParseStrval(ptr, end, res);
    }
}

ParseResult ServerProtocol::_ParseStrval(const char*& ptr, const char* end, std::string& res) {
    assert (paramLen_ >= 0);

    if (static_cast<int>(end - ptr) < paramLen_ + 2)
        return ParseResult::wait;

    const char* const start = ptr;
    auto tail = ptr + paramLen_;
    if (tail[0] != '\r' || tail[1] != '\n')
        return ParseResult::error;

    res.assign(ptr, tail - ptr);
    ptr = tail + 2;
    paramLen_ = -1;

    content_.append(start, ptr);
    return ParseResult::ok;
}

ParseResult ServerProtocol::_ParseStrlen(const char*& ptr, const char* end, int& result) {
    if (end - ptr < 3)
        return ParseResult::wait;

    if (*ptr != '$')
        return ParseResult::error;

    ++ ptr;

    const auto ret = GetIntUntilCRLF(ptr,  end - ptr, result);
    if (ret != ParseResult::ok)
        -- ptr;

    return ret;
}

// ClientProtocol
ClientProtocol::ClientProtocol() {
    Reset();
}

ParseResult ClientProtocol::Parse(const char*& ptr, const char* end) {
    assert (end - ptr >= 1);

    const char* data = ptr;
    if (type_ == ResponseType::None) {
        switch (data[0]) {
        case '+':
            type_ = ResponseType::Fine;
            break;

        case '-':
            type_ = ResponseType::Error;
            break;

        case '$':
            type_ = ResponseType::String;
            break;

        case ':':
            type_ = ResponseType::Number;
            break;

        case '*':
            type_ = ResponseType::Multi;
            break;

        default:
            return ParseResult::error;
        }
    }

    bool ready = false;
    switch (type_) {
    case ResponseType::Fine:
    case ResponseType::Error:
    case ResponseType::Number: {
        const char* res = SearchCRLF(data + 1, end - (data + 1));
        if (res) {
            data = res + 2;
            ready = true;
        } else {
            return ParseResult::wait;
        }
    }
    break;

    case ResponseType::String: {
        auto ret = _ParseStr(data, end);
        if (ret == ParseResult::error)
            return ret;
        else if (ret == ParseResult::ok)
            ready = true;
    }

    break;

    case ResponseType::Multi: {
        auto ret = _ParseMulti(data, end);
        if (ret == ParseResult::error)
            return ret;
        else if (ret == ParseResult::ok)
            ready = true;
    }
    break;

    default:
        assert (!!!"bug wrong type");
        break;
    }

    if (ready) {
        content_.append(ptr, data);
        ptr = data;
    }

    return ParseResult::ok;
}

void ClientProtocol::Reset() {
    type_ = ResponseType::None;
    content_.clear();
    multi_ = kInvalid;
    paramLen_ = kInvalid;
    nParams_ = 0;
    params_.clear();
}

ParseResult ClientProtocol::_ParseStr(const char*& ptr, const char* end) {
    if (paramLen_ == kInvalid) {
        auto parseRet = _ParseStrlen(ptr, end, paramLen_);
        if (parseRet == ParseResult::error ||
                paramLen_ < -1)
            return ParseResult::error;

        if (parseRet != ParseResult::ok)
            return ParseResult::wait;
    }

    if (paramLen_ == -1) {
        // fix bug
        paramLen_ = kInvalid;
        return ParseResult::ok;
    } else
        return _ParseStrval(ptr, end);
}

ParseResult ClientProtocol::_ParseStrval(const char*& ptr, const char* end) {
    assert (paramLen_ >= 0);

    if (static_cast<int>(end - ptr) < paramLen_ + 2)
        return ParseResult::wait;

    auto tail = ptr + paramLen_;
    if (tail[0] != '\r' || tail[1] != '\n')
        return ParseResult::error;

    params_.emplace_back(ptr, tail - ptr);
    ptr = tail + 2;
    paramLen_ = kInvalid;

    return ParseResult::ok;
}

ParseResult ClientProtocol::_ParseStrlen(const char*& ptr, const char* end, int& result) {
    if (end - ptr < 3)
        return ParseResult::wait;

    if (*ptr != '$')
        return ParseResult::error;

    ++ ptr;

    const auto ret = GetIntUntilCRLF(ptr,  end - ptr, result);
    if (ret != ParseResult::ok)
        -- ptr;

    return ret;
}

ParseResult ClientProtocol::_ParseMulti(const char*& ptr, const char* end) {
    if (multi_ == kInvalid) {
        if (end - ptr < 3)
            return ParseResult::wait;

        if (*ptr != '*')
            return ParseResult::error;

        ++ ptr;
        auto ret = GetIntUntilCRLF(ptr,  end - ptr, multi_);
        if (ret == ParseResult::error || multi_ < -1)
            return ParseResult::error;
        if (ret != ParseResult::ok)
            return ParseResult::wait;
    }

    while (nParams_ < multi_) {
        auto ret = _ParseStr(ptr, end);
        if (ret == ParseResult::ok)
            ++ nParams_;
        else
            return ret;
    }

    return ParseResult::ok;
}

#if 0
#include <iostream>

int main() {
    std::string rsp = "*2\r\n$-1\r\n$-1\r\n";
    //std::string rsp = "$3\r\nabc\r\n";
    //std::string rsp = "*2\r\n$4\r\nname\r\n$4\r\nbert\r\n";
#if 0
    ServerProtocol proto;
#else
    ClientProtocol proto;
#endif

    const char* start = rsp.data();
    while (start < rsp.data() + rsp.size()) {
        //const char* start = rsp.data();
        auto ret = proto.Parse(start, start + rsp.size());
        //auto ret = proto.ParseRequest(start, rsp.data() + rsp.size());
        if (ret == ParseResult::error)
            return -1;
    }

    std::cout << "start - rsp.data() " << start - rsp.data() << std::endl;
    std::cout << "result " << proto.GetContent()  << std::endl;
    //std::cout << "result " << proto.GetRawRequest()  << std::endl;
    //for (const auto& e : proto.GetParams())
    //   std::cout << e << std::endl;
}

#endif

