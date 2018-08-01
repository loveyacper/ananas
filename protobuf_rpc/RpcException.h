#ifndef BERT_RPCEXCEPTION_H
#define BERT_RPCEXCEPTION_H

#include <string>
#include <stdexcept>

namespace ananas {

namespace rpc {

class NoServiceException : public std::logic_error {
public:
    NoServiceException(const std::string& e = "") :
        std::logic_error(e) {
    }
};

class NoMethodException : public std::logic_error {
public:
    NoMethodException(const std::string& e = "") :
        std::logic_error(e) {
    }
};

class NoRequestException : public std::invalid_argument {
public:
    NoRequestException(const std::string& e = "") :
        std::invalid_argument(e) {
    }
};

class MethodUndeterminedException : public std::runtime_error {
public:
    MethodUndeterminedException(const std::string& e) :
        std::runtime_error(e) {
    }
};

} // end namespace rpc

} // end namespace ananas

#endif

