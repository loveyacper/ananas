


#include "Protocol.h"

#include <cassert>

// 1 request -> multi strlist
// 2 multi -> * number crlf
// 3 strlist -> str strlist | empty
// 4 str -> strlen strval
// 5 strlen -> $ number crlf
// 6 strval -> string crlf

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


void Protocol::Reset() {
    multi_ = -1;
    paramLen_ = -1;
    params_.clear();
}

ParseResult Protocol::ParseRequest(const char*& ptr, const char* end) {
    if (multi_ == -1) {
        auto parseRet = _ParseMulti(ptr, end, multi_);
        if (parseRet == ParseResult::error ||
                multi_ < -1)
            return ParseResult::error;

        if (parseRet != ParseResult::ok)
            return ParseResult::wait;
    }

    return _ParseStrlist(ptr, end, params_);
}

ParseResult Protocol::_ParseMulti(const char*& ptr, const char* end, int& result) {
    if (end - ptr < 3)
        return ParseResult::wait;

    if (*ptr != '*')
        return ParseResult::error;

    ++ ptr;

    return GetIntUntilCRLF(ptr,  end - ptr, result);
}

ParseResult Protocol::_ParseStrlist(const char*& ptr, const char* end, std::vector<std::string>& results) {
    while (static_cast<int>(results.size()) < multi_) {
        std::string res;
        auto parseRet = _ParseStr(ptr, end, res);

        if (parseRet == ParseResult::ok) {
            results.emplace_back(std::move(res));
        } else {
            return parseRet;
        }
    }

    return ParseResult::ok;
}

ParseResult Protocol::_ParseStr(const char*& ptr, const char* end, std::string& result) {
    if (paramLen_ == -1) {
        auto parseRet = _ParseStrlen(ptr, end, paramLen_);
        if (parseRet == ParseResult::error ||
                paramLen_ < -1)
            return ParseResult::error;

        if (parseRet != ParseResult::ok)
            return ParseResult::wait;
    }

    return _ParseStrval(ptr, end, result);
}

ParseResult Protocol::_ParseStrval(const char*& ptr, const char* end, std::string& result) {
    assert (paramLen_ >= 0);

    if (static_cast<int>(end - ptr) < paramLen_ + 2)
        return ParseResult::wait;

    auto tail = ptr + paramLen_;
    if (tail[0] != '\r' || tail[1] != '\n')
        return ParseResult::error;

    result.assign(ptr, tail - ptr);
    ptr = tail + 2;
    paramLen_ = -1;

    return ParseResult::ok;
}

ParseResult Protocol::_ParseStrlen(const char*& ptr, const char* end, int& result) {
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

