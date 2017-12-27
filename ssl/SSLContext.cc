#include <cassert>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "SSLContext.h"
#include "SSLManager.h"
#include "net/Connection.h"
#include "net/AnanasDebug.h"
#include "util/log/Logger.h"

//#define TEST_SSL_RENEGO
#ifdef TEST_SSL_RENEGO
#include "net/EventLoop.h" // for test
#endif

namespace ananas
{
namespace ssl
{

OpenSSLContext::OpenSSLContext(SSL* ssl, bool incoming, void* data) :
    ssl_(ssl),
    incoming_(incoming)
{
    SSL_set_ex_data(ssl_, 0, data);
}

OpenSSLContext::~OpenSSLContext()
{
    if (ssl_) SSL_free(ssl_);
}

bool OpenSSLContext::SendData(const char* data, size_t len)
{
    DBG(internal::g_debug) << "SendData: " << len;

    if (len == 0)
        return true;

    bool driveByLoop = (!sendBuffer_.IsEmpty()) && (sendBuffer_.ReadAddr() == data);

    if (!driveByLoop && !sendBuffer_.IsEmpty())
    {
        DBG(internal::g_debug) << "!driveByLoop && !sendBuffer_.IsEmpty()";
        // may writeWaitReadable_ == true
        sendBuffer_.PushData(data, len);
        return true;
    }

    if (!driveByLoop && (readWaitReadable_ || shutdownWaitReadable_))
    {
        DBG(internal::g_debug) << readWaitReadable_;
        assert (!writeWaitReadable_);
        sendBuffer_.PushData(data, len);
        return true;
    }

    ERR_clear_error();
    int ret = SSL_write(ssl_, data, len);
    if (ret <= 0)
    {
        int err = SSL_get_error(ssl_, ret);
        assert (err != SSL_ERROR_WANT_WRITE);

        WRN(internal::g_debug) << "SSL_write error: " << err << SSL_state_string_long(ssl_);

        if (err == SSL_ERROR_WANT_READ)
        {
            // If active renegotiate, then call SSL_write,
            // write to mem bio will always success, so there are some data to send.
            // and return SSL_ERROR_WANT_READ, because I want your handshake response.
            writeWaitReadable_ = true;
            if (!driveByLoop)
                sendBuffer_.PushData(data, len);
        }
        else
        {
            return false;
        }
    }
    else
    {
        INF(internal::g_debug) << "SSL_write state: " << SSL_state_string_long(ssl_);
        writeWaitReadable_ = false;
        if (driveByLoop)
        {
            DBG(internal::g_debug) << "driveByLoop send buffer bytes " << sendBuffer_.ReadableSize();
            sendBuffer_.Clear();
        }
        else
        {
            DBG(internal::g_debug) << "!driveByLoop && writeWaitReadable_ = false";
        }
    }

    Buffer buf = GetMemData(SSL_get_wbio(ssl_));
    if (!buf.IsEmpty())
    {
        DBG(internal::g_debug) << "SSL_write send bytes " << buf.ReadableSize() << ", and buffer has " << sendBuffer_.ReadableSize();

        auto c = (ananas::Connection*)SSL_get_ex_data(ssl_, 0);
        return c->SendPacket(buf.ReadAddr(), buf.ReadableSize());
    }
            
    DBG(internal::g_debug) << "no data send";

    return true;
}

void OpenSSLContext::SetLogicProcess(std::function<size_t (Connection* , const char*, size_t )> cb)
{
    logicProcess_ = std::move(cb);
}

Buffer OpenSSLContext::GetMemData(BIO* bio)
{
    Buffer buf;
    while (true)
    {
        buf.AssureSpace(16 * 1024);
        int bytes = BIO_read(bio, buf.WriteAddr(), buf.WritableSize());
        if (bytes <= 0)
            return buf;

        buf.Produce(bytes);
    }
            
    // never here
    return buf;
}

size_t OpenSSLContext::ProcessHandshake(std::shared_ptr<OpenSSLContext> open, Connection* c, const char* data, size_t len)
{
    // write back to ssl read buffer
    SSL* ssl = open->ssl_;
    BIO_write(SSL_get_rbio(ssl), data, len);

    // process handshake
    int ret = open->incoming_ ? SSL_accept(ssl) : SSL_connect(ssl);
    if (ret == 1)
    { 
        DBG(internal::g_debug) << "ProcessHandshake is OK!";
        //  handshake is OK, just for test.
        c->SetOnMessage(std::bind(&OpenSSLContext::ProcessData, open,
                                                                std::placeholders::_1,
                                                                std::placeholders::_2,
                                                                std::placeholders::_3));
    }
    else
    {
        int err = SSL_get_error(ssl, ret);

        if (err != SSL_ERROR_WANT_READ)
        {
            DBG(internal::g_debug) << "SSL_connect failed " << err;
            c->ActiveClose();
            return len;
        }
    }

    // send the encrypt data from write buffer
    Buffer toSend = GetMemData(SSL_get_wbio(ssl));
    c->SendPacket(toSend.ReadAddr(), toSend.ReadableSize());

    return len;
}

size_t OpenSSLContext::ProcessData(std::shared_ptr<OpenSSLContext> open, Connection* c, const char* data, size_t len)
{
    DBG(internal::g_debug) << "OpenSSLContext::onMessage len " << len;

    // write back to ssl read buffer
    SSL* ssl = open->ssl_;
    BIO_write(SSL_get_rbio(ssl), data, len);

    if (open->writeWaitReadable_)
    {
        DBG(internal::g_debug) << "Epollin, but writeWaitReadable_ == true";
        assert (!open->sendBuffer_.IsEmpty());
        if (!open->SendData(open->sendBuffer_.ReadAddr(), open->sendBuffer_.ReadableSize()))
        {
            ERR(internal::g_debug) << "Epollin, but sendData failed";
            c->ActiveClose();
        }

        return len;
    }
    else
    {
        // read to plain text buffer, because ssl record may be split to multi packages
        open->recvPlainBuf_.AssureSpace(16 * 1024);
        ERR_clear_error();
        INF(internal::g_debug) << "SSL_read before state:" << SSL_state_string_long(ssl);
        int bytes = SSL_read(ssl, open->recvPlainBuf_.WriteAddr(), open->recvPlainBuf_.WritableSize());
        INF(internal::g_debug) << "SSL_read after state:" << SSL_state_string_long(ssl);

        if (SSL_is_init_finished(ssl))
            USR(internal::g_debug) << "SSL_read finished true";

        if (bytes > 0)
        {
            open->recvPlainBuf_.Produce(bytes);
            open->readWaitReadable_ = false;
            // now process logic packet
            int processed = open->logicProcess_(c, open->recvPlainBuf_.ReadAddr(), open->recvPlainBuf_.ReadableSize());
            if (processed > 0)
                open->recvPlainBuf_.Consume(processed);

        }
        else
        {
            int err = SSL_get_error(ssl, bytes);
                
            // when peer issued renegotiation, here will demand us to send handshake data.
            // write to mem bio will always success, only need to check whether has data to send.
            assert (err != SSL_ERROR_WANT_WRITE);

            WRN(internal::g_debug) << "SSL_read error " << err;

            if (err == SSL_ERROR_WANT_READ)
            {
                // why report SSL_ERROR_WANT_READ when SSL_is_init_finished return 1?
                if (SSL_is_init_finished(ssl))
                    open->readWaitReadable_ = false;
                else
                    open->readWaitReadable_ = true;
            }
            else
            {
                c->ActiveClose();
                return len;
            }
        }

        // send the encrypt data from write buffer
        Buffer data = GetMemData(SSL_get_wbio(ssl));
        if (!data.IsEmpty())
        {
            DBG(internal::g_debug) << "SSL_read status " << SSL_get_error(ssl, bytes) << ", but has to send data bytes " << data.ReadableSize();
            c->SendPacket(data.ReadAddr(), data.ReadableSize());
        }
    }

    return len;
}

bool OpenSSLContext::DoHandleShake()
{
    if (!SSL_is_init_finished(ssl_))
    {
        ERR(internal::g_debug) << "Can not DoHandleShake when SSL_is_init_finished 0";
        return false;
    }

    int ret = SSL_renegotiate(ssl_);
    INF(internal::g_debug) << "SSL_renegotiate ret " << ret;

    if (ret != 1) return false;

    auto c = (ananas::Connection*)SSL_get_ex_data(ssl_, 0);

    if (incoming_)
    {
        // server side
        int ret = SSL_do_handshake(ssl_);
        if (ret != 1)
        {
            ERR(internal::g_debug) << "server SSL_do_handshake error " << SSL_get_error(ssl_, ret) << " and state:" << SSL_state_string_long(ssl_);

            c->ActiveClose();
            return false;
        }
        else
        {
            ssl_->state = SSL_ST_ACCEPT;
            INF(internal::g_debug) << "server SSL_ST_ACCEPT and state:" << SSL_state_string_long(ssl_);
        }
    }
    
    ret = SSL_do_handshake(ssl_);
    DBG(internal::g_debug) << "SSL_do_handshake ret " << ret << " and state:" << SSL_state_string_long(ssl_);
    if (ret <= 0)
    {
        int err = SSL_get_error(ssl_, ret);
        INF(internal::g_debug) << "SSL_do_handshake error " << err;
        if (err != SSL_ERROR_WANT_READ)
        {
            c->ActiveClose();
            return false;
        }
    }
            
    Buffer buf = OpenSSLContext::GetMemData(SSL_get_wbio(ssl_));
    if (!buf.IsEmpty())
    {
        DBG(internal::g_debug) << "SSL_renegotiate send bytes " << buf.ReadableSize();
        c->SendPacket(buf.ReadAddr(), buf.ReadableSize());
    }

    return true;
}

void OpenSSLContext::Shutdown()
{
    int err = SSL_shutdown(ssl_);
    
    if (err == 0)
        SSL_shutdown(ssl_);

    SSL_free(ssl_);
    ssl_ = nullptr;
}
    
void OnNewSSLConnection(const std::string& ctxName, int verifyMode, bool incoming, Connection* c)
{
    SSL_CTX* ctx = SSLManager::Instance().GetCtx(ctxName);
    if (!ctx)
    {
        ERR(internal::g_debug) << "not find ctx " << ctxName;
        c->ActiveClose();
        return;
    }

    SSL* ssl = SSL_new(ctx);

    SSL_set_mode(ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER); // allow retry ssl-write with different args
    SSL_set_verify(ssl, verifyMode, NULL);
    SSL_set_bio(ssl, BIO_new(BIO_s_mem()), BIO_new(BIO_s_mem()));

    BIO_set_mem_eof_return(SSL_get_rbio(ssl), -1);
    BIO_set_mem_eof_return(SSL_get_wbio(ssl), -1);

    auto open = std::make_shared<OpenSSLContext>(ssl, incoming, c);
    c->SetUserData(open);
    c->SetOnDisconnect(std::bind([](OpenSSLContext* ctx, Connection* c) {
                ctx->Shutdown();
            }, open.get(), std::placeholders::_1));
#if 0
    open->SetLogicProcess([](Connection* c, const char* data, size_t len) {
            DBG(internal::g_debug) << "Process len " << len;
            DBG(internal::g_debug) << "Process data " << data;
            return len;
    });
#endif

    const int kSSLHeadSize = 5;
    c->SetMinPacketSize(kSSLHeadSize);
    c->SetOnMessage(std::bind(&OpenSSLContext::ProcessHandshake, open,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2,
                                                                 std::placeholders::_3));

    int ret = incoming ? SSL_accept(ssl) : SSL_connect(ssl);
    if (ret <= 0)
    {
        int err = SSL_get_error(ssl, ret);

        if (err != SSL_ERROR_WANT_READ)
        {
            ERR(internal::g_debug) << "SSL_connect failed " << err;
            c->ActiveClose();
            return;
        }
    }

    // use mem bio, can't success for now
    assert (ret != 1);

    // send the encrypt data from write buffer
    Buffer data = OpenSSLContext::GetMemData(SSL_get_wbio(ssl));
    c->SendPacket(data.ReadAddr(), data.ReadableSize());

    // !!!  test ssl_write when renegotiate !!!
#ifdef TEST_SSL_RENEGO
    if (!incoming) 
        return;

    auto loop = c->GetLoop();
    loop->ScheduleAfter(std::chrono::seconds(2), [=]() {
            // assume ssl_handshake must be done after 2 secs.
            if (!open->DoHandleShake())
                ERR(internal::g_debug) << "DoHandleShake failed";

            loop->ScheduleAfter(std::chrono::seconds(1), [=]() { 
                open->SendData("haha", 4); 
            });
        });
#endif
}

} // end namespace ssl

} // end namespace ananas

