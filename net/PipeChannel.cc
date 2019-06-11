#include <unistd.h>
#include <cassert>
#include <fcntl.h>

#include "PipeChannel.h"

namespace ananas {

namespace internal {

PipeChannel::PipeChannel() {
    int fd[2];
    int ret = ::pipe(fd);
    assert (ret == 0);
    readFd_ = fd[0];
    writeFd_ = fd[1];
    SetPipeNonBlock(readFd_);
    SetPipeNonBlock(writeFd_);
}

PipeChannel::~PipeChannel() {
    ::close(readFd_);
    ::close(writeFd_);
}

int PipeChannel::Identifier() const {
    return readFd_;
}

bool PipeChannel::HandleReadEvent() {
    char ch;
    auto n = ::read(readFd_, &ch, sizeof ch);
    return n == 1;
}

bool PipeChannel::HandleWriteEvent() {
    assert (false);
    return false;
}

void PipeChannel::HandleErrorEvent() {
}

bool PipeChannel::Notify() {
    char ch = 0;
    auto n = ::write(writeFd_, &ch, sizeof ch);
    return n == 1;
}

void SetPipeNonBlock(int pipeFd, bool nonBlock) {
    int flag = ::fcntl(pipeFd, F_GETFL, 0);
    assert(flag >= 0 && "Pipe Non Block failed");

    if (nonBlock) 
        flag = ::fcntl(pipeFd, F_SETFL, flag | O_NONBLOCK);
    else
        flag = ::fcntl(pipeFd, F_SETFL, flag & ~O_NONBLOCK);
}

} // end namespace internal

} // end namespace ananas

