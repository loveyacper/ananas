#include "SSLManager.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace ananas
{

namespace ssl
{

SSLManager::SSLManager()
{
}

SSLManager& SSLManager::Instance()
{
    static SSLManager mgr;
    return mgr;
}

void SSLManager::GlobalInit()
{
    (void)SSL_library_init();
    OpenSSL_add_all_algorithms();

    ERR_load_ERR_strings();
    SSL_load_error_strings();
}


SSLManager::~SSLManager()
{ 
    for (const auto& e : ctxSet_)
        SSL_CTX_free(e.second);

    ERR_free_strings();
    EVP_cleanup();
}

bool SSLManager::AddCtx(const std::string& name,
                        const std::string& cafile, 
                        const std::string& certfile, 
                        const std::string& keyfile)
{
    if (ctxSet_.count(name))
        return false;
    
    // init support all ssl/tls methods
    SSL_CTX* ctx = SSL_CTX_new(SSLv23_method());
    if (!ctx)
        return false;

    // disable the insecure SSLV2 & SSLV3
    SSL_CTX_clear_options(ctx, SSL_OP_NO_SSLv2);
    SSL_CTX_clear_options(ctx, SSL_OP_NO_SSLv3);

#define RETURN_IF_FAIL(call) \
    if ((call) <= 0) { \
        ERR_print_errors_fp(stderr); \
        return false;\
    }

    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);

    RETURN_IF_FAIL (SSL_CTX_set_session_id_context(ctx, (const unsigned char*)ctx, sizeof ctx));
    RETURN_IF_FAIL (SSL_CTX_load_verify_locations(ctx, cafile.data(), nullptr));
    RETURN_IF_FAIL (SSL_CTX_use_certificate_file(ctx, certfile.data(), SSL_FILETYPE_PEM));
    RETURN_IF_FAIL (SSL_CTX_use_PrivateKey_file(ctx, keyfile.data(), SSL_FILETYPE_PEM));
    RETURN_IF_FAIL (SSL_CTX_check_private_key(ctx));

#undef RETURN_IF_FAIL

    return ctxSet_.insert({name, ctx}).second;
}

SSL_CTX* SSLManager::GetCtx(const std::string& name) const
{
    auto it  = ctxSet_.find(name);
    return it == ctxSet_.end() ? nullptr : it->second;
}

} // end namespace ssl

} // end namespace ananas

