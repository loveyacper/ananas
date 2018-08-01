#ifndef BERT_PIPECHANNEL_H
#define BERT_PIPECHANNEL_H

#include "Poller.h"

namespace ananas {

namespace internal {

class PipeChannel : public internal::Channel {
public:
    PipeChannel();
    ~PipeChannel();

    PipeChannel(const PipeChannel& ) = delete;
    void operator= (const PipeChannel& ) = delete;

    int Identifier() const override;
    bool HandleReadEvent() override;
    bool HandleWriteEvent() override;
    void HandleErrorEvent() override;

    bool Notify();

private:
    int readFd_;
    int writeFd_;
};

} // end namespace internal

} // end namespace ananas

#endif

