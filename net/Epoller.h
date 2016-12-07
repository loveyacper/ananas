#ifndef BERT_EPOLLER_H
#define BERT_EPOLLER_H

#ifdef __gnu_linux__

#include <sys/epoll.h>
#include <vector>
#include "Poller.h"

namespace ananas
{
namespace internal
{

class Epoller : public Poller
{
public:
    Epoller();
   ~Epoller();

    Epoller(const Epoller& ) = delete;
    void operator= (const Epoller& ) = delete;

    bool Register(int fd, int events, void* userPtr) override;
    bool Modify(int fd, int events, void* userPtr) override;
    bool Unregister(int fd, int events) override;

    int Poll(std::size_t maxEvent, int timeoutMs) override;

private:
    std::vector<epoll_event> events_;
};

} // end namespace internal
} // end namespace ananas

#endif // end #ifdef __gnu_linux__

#endif

