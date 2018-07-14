#ifndef BERT_KQUEUE_H
#define BERT_KQUEUE_H

#if defined(__APPLE__)

#include <vector>
#include <sys/event.h>
#include "Poller.h"

namespace ananas
{
namespace internal
{

class Kqueue : public Poller
{
public:
    Kqueue();
   ~Kqueue();

    bool Register(int fd, int events, void* userPtr) override;
    bool Modify(int fd, int events, void* userPtr) override;
    bool Unregister(int fd, int events) override;

    int Poll(std::size_t maxEvent, int timeoutMs) override;

private:
    std::vector<struct kevent> events_;    
};

} // end namespace internal
} // end namespace ananas

#else

void __Dummy__();

#endif  // end #if defined(__APPLE__)

#endif

