#ifndef BERT_SSLMANAGER_H
#define BERT_SSLMANAGER_H

#include <unordered_map>
#include <string>

struct ssl_ctx_st;
typedef struct ssl_ctx_st SSL_CTX;

struct ssl_method_st;
typedef struct ssl_method_st SSL_METHOD;

namespace ananas {

namespace ssl {

class SSLManager {
public:
    static SSLManager& Instance();
    static void GlobalInit();

    ~SSLManager();

    bool AddCtx(const std::string& name,
                const std::string& cafile,
                const std::string& certfile,
                const std::string& keyfile);

    SSL_CTX* GetCtx(const std::string& name) const;

private:
    SSLManager();

    std::unordered_map<std::string, SSL_CTX*> ctxSet_;
};

} // end namespace ssl

} // end namespace ananas

#endif

