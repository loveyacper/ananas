#ifndef BERT_SSLCONTEXT_H
#define BERT_SSLCONTEXT_H

#include <memory>
#include <string>
#include <functional>
#include "net/Buffer.h"


struct ssl_st;
typedef struct ssl_st SSL;

namespace ananas
{

class Connection;

namespace ssl
{

void OnNewSSLConnection(const std::string& ctx, int verifyMode, bool incoming, Connection* conn);

class OpenSSLContext
{
    friend void OnNewSSLConnection(const std::string& , int , bool , Connection* );
public:
    explicit
    OpenSSLContext(SSL* ssl, bool incoming_, void* exData);
    ~OpenSSLContext();

    bool SendData(const char* data, size_t len);
    void SetLogicProcess(std::function<size_t (Connection* , const char*, size_t )>);

private:
    static Buffer getMemData(BIO* bio);
    static size_t processHandshake(std::shared_ptr<OpenSSLContext> open, Connection* c, const char* data, size_t len);
    static size_t processData(std::shared_ptr<OpenSSLContext> open, Connection* c, const char* data, size_t len);

    std::function<size_t (Connection* c, const char* data, size_t len)> logicProcess_;

    SSL* const ssl_;
    const bool incoming_;

    Buffer recvPlainBuf_;

    // use memory bio, never SSL_ERROR_WANT_WRITE.

    // SSL_read encounter SSL_ERROR_WANT_READ.
    bool readWaitReadable_ {false};

    // SSL_write encounter SSL_ERROR_WANT_READ!
    bool writeWaitReadable_ {false};
    Buffer sendBuffer_;

    // TODO 
    // SSL_shutdown encounter SSL_ERROR_WANT_READ!
    bool shutdownWaitReadable_ {false};
};



} // end namespace ssl

} // end namespace ananas

#endif

