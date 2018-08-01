#ifndef BERT_PROTOCOL_H
#define BERT_PROTOCOL_H

#include <vector>
#include <string>


enum class ParseResult : int8_t {
    ok,
    wait,
    error,
};

class Protocol {
public:
    void Reset();
    ParseResult ParseRequest(const char*& ptr, const char* end);

    const std::vector<std::string>& GetParams() const {
        return params_;
    }

    bool IsInitialState() const {
        return multi_ == -1;
    }

private:
    ParseResult _ParseMulti(const char*& ptr, const char* end, int& result);
    ParseResult _ParseStrlist(const char*& ptr, const char* end, std::vector<std::string>& results);
    ParseResult _ParseStr(const char*& ptr, const char* end, std::string& result);
    ParseResult _ParseStrval(const char*& ptr, const char* end, std::string& result);
    ParseResult _ParseStrlen(const char*& ptr, const char* end, int& result);

    int multi_ = -1;
    int paramLen_ = -1;
    std::vector<std::string> params_;
};

#endif
