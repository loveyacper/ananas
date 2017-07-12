#include <cassert>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "SSLContext.h"
#include "SSLManager.h"
#include "net/Connection.h"
#include "net/log/Logger.h"
#include "net/AnanasDebug.h"
#include "net/EventLoop.h" // for test

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
    SSL_free(ssl_);
}

bool OpenSSLContext::SendData(const char* data, size_t len)
{
    if (len == 0)
        return true;

    bool driveByLoop = (!sendBuffer_.IsEmpty()) && (sendBuffer_.ReadAddr() == data);

    if (!driveByLoop && !sendBuffer_.IsEmpty())
    {
        // may writeWaitReadable_ == true
        sendBuffer_.PushData(data, len);
        return true;
    }

    if (!driveByLoop && (readWaitReadable_ || shutdownWaitReadable_))
    {
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

        ERR_print_errors_fp(stderr);

        WRN(internal::g_debug) << "SSL_write error: " << err;

        if (err == SSL_ERROR_WANT_READ)
        {
            // If active renegotiate, then call SSL_write,
            // write to mem bio will always success, so there are some data to send.
            // and return SSL_ERROR_WANT_READ, because I wanna your handshake response.
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
        writeWaitReadable_ = false;
        if (driveByLoop)
        {
            DBG(internal::g_debug) << "driveByLoop send buffer bytes " << sendBuffer_.ReadableSize();
            sendBuffer_.Clear();
        }
    }

    Buffer buf = getMemData(SSL_get_wbio(ssl_));
    if (!buf.IsEmpty())
    {
        DBG(internal::g_debug) << "SSL_write send bytes " << buf.ReadableSize();

        auto c = (ananas::Connection*)SSL_get_ex_data(ssl_, 0);
        return c->SendPacket(buf.ReadAddr(), buf.ReadableSize());
    }

    return true;
}

void OpenSSLContext::SetLogicProcess(std::function<size_t (Connection* , const char*, size_t )> cb)
{
    logicProcess_ = std::move(cb);
}

//static 
Buffer OpenSSLContext::getMemData(BIO* bio)
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

//static 
size_t OpenSSLContext::processHandshake(std::shared_ptr<OpenSSLContext> open, Connection* c, const char* data, size_t len)
{
    // write back to ssl read buffer
    SSL* ssl = open->ssl_;
    BIO_write(SSL_get_rbio(ssl), data, len);

    // process handshake
    int ret = open->incoming_ ? SSL_accept(ssl) : SSL_connect(ssl);
    if (ret == 1)
    { 
        DBG(internal::g_debug) << "processHandshake is OK!";
        //  handshake is OK, just for test.
        open->SetLogicProcess([](Connection* c, const char* data, size_t len) {
                                                                              DBG(internal::g_debug) << "Process len " << len;
                                                                              DBG(internal::g_debug) << "Process data " << data;
                                                                              return len;
                                                                            });
        c->SetOnMessage(std::bind(&OpenSSLContext::processData, open,
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
    Buffer toSend = getMemData(SSL_get_wbio(ssl));
    c->SendPacket(toSend.ReadAddr(), toSend.ReadableSize());

    return len;
}

static void GetSSLHead(const char* data, int8_t& type, uint16_t& ver, uint16_t& len)
{
    type = data[0];
    ver = *(uint16_t*)(data + 1);
    len = *(uint16_t*)(data + 3);

    ver = ntohs(ver);
    len = ntohs(len);
}

const size_t kSSLHeadSize = 5;

//static 
size_t OpenSSLContext::processData(std::shared_ptr<OpenSSLContext> open, Connection* c, const char* data, size_t len)
{
    int8_t type;
    uint16_t ver, pkgLen;
    GetSSLHead(data, type, ver, pkgLen);
            
    DBG(internal::g_debug) << "OpenSSLContext::processData type " << (int)type << ", ver " << ver << ", len " << pkgLen;

    const size_t totalLen = kSSLHeadSize + static_cast<size_t>(pkgLen);
    if (len < totalLen)
        return 0;

    // write back to ssl read buffer
    SSL* ssl = open->ssl_;
    BIO_write(SSL_get_rbio(ssl), data, totalLen);

    if (open->writeWaitReadable_)
    {
        DBG(internal::g_debug) << "Epollin, but writeWaitReadable_ == true";
        assert (!open->sendBuffer_.IsEmpty());
        if (!open->SendData(open->sendBuffer_.ReadAddr(), open->sendBuffer_.ReadableSize()))
        {
            ERR(internal::g_debug) << "Epollin, but sendData failed";
            c->ActiveClose();
        }

        return totalLen;
    }
    else
    {
        // read to plain text buffer, because ssl record may be split to multi packages
        open->recvPlainBuf_.AssureSpace(16 * 1024);
        ERR_clear_error();
        int bytes = SSL_read(ssl, open->recvPlainBuf_.WriteAddr(), open->recvPlainBuf_.WritableSize());
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
                // a big package may be split into multiple ssl records.
                open->readWaitReadable_ = true;
            }
            else
            {
                c->ActiveClose();
                return totalLen;
            }
        }

        // send the encrypt data from write buffer
        Buffer data = getMemData(SSL_get_wbio(ssl));
        if (!data.IsEmpty())
        {
            DBG(internal::g_debug) << "SSL_read status " << SSL_get_error(ssl, bytes) << ", but has to send data bytes " << data.ReadableSize();
            c->SendPacket(data.ReadAddr(), data.ReadableSize());
        }
    }

    return totalLen;
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
    auto open = std::make_shared<OpenSSLContext>(ssl, incoming, c);
        
    SSL_set_verify(ssl, verifyMode, NULL);
    SSL_set_bio(ssl, BIO_new(BIO_s_mem()), BIO_new(BIO_s_mem()));

    c->SetMinPacketSize(kSSLHeadSize);
    c->SetOnMessage(std::bind(&OpenSSLContext::processHandshake, open,
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
    Buffer data = OpenSSLContext::getMemData(SSL_get_wbio(ssl));
    c->SendPacket(data.ReadAddr(), data.ReadableSize());

    // !!!  TODO TEST test ssl_write when renegotiate !!!
    auto loop = c->GetLoop();
    loop->ScheduleAfter(std::chrono::seconds(2), [=]() {
            // assume ssl_handshake must be done after 2 secs.
            int ret = SSL_renegotiate(ssl);
            ERR(internal::g_debug) << "SSL_renegotiate ret " << ret;

            open->SendData("hello,", 6);
            open->SendData("world!", 6);
        });
}

} // end namespace ssl

} // end namespace ananas

