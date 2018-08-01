#ifdef __gnu_linux__

#include "Epoller.h"

#include <errno.h>
#include <unistd.h>

#include "AnanasDebug.h"

namespace ananas {
namespace internal {

namespace Epoll {
bool ModSocket(int epfd, int socket, uint32_t events, void* ptr);

bool AddSocket(int epfd, int socket, uint32_t events, void* ptr) {
    if (socket < 0)
        return false;

    epoll_event  ev;
    ev.data.ptr= ptr;
    ev.events = 0;

    if (events & eET_Read)
        ev.events |= EPOLLIN;
    if (events & eET_Write)
        ev.events |= EPOLLOUT;

    return 0 == epoll_ctl(epfd, EPOLL_CTL_ADD, socket, &ev);
}

bool DelSocket(int epfd, int socket) {
    if (socket < 0)
        return false;

    epoll_event dummy;
    return 0 == epoll_ctl(epfd, EPOLL_CTL_DEL, socket, &dummy) ;
}

bool ModSocket(int epfd, int socket, uint32_t events, void* ptr) {
    if (socket < 0)
        return false;

    epoll_event  ev;
    ev.data.ptr= ptr;
    ev.events = 0;

    if (events & eET_Read)
        ev.events |= EPOLLIN;
    if (events & eET_Write)
        ev.events |= EPOLLOUT;

    return 0 == epoll_ctl(epfd, EPOLL_CTL_MOD, socket, &ev);
}
}


Epoller::Epoller() {
    multiplexer_ = ::epoll_create(512);
    ANANAS_DBG << "create epoll: " << multiplexer_;
}

Epoller::~Epoller() {
    if (multiplexer_ != -1) {
        ANANAS_DBG << "close epoll:  " << multiplexer_;
        ::close(multiplexer_);
    }
}

bool Epoller::Register(int fd, int events, void* userPtr) {
    if (Epoll::AddSocket(multiplexer_, fd, events, userPtr))
        return true;

    return (errno == EEXIST) && Modify(fd, events, userPtr);
}

bool Epoller::Unregister(int fd, int events) {
    return Epoll::DelSocket(multiplexer_, fd);
}


bool Epoller::Modify(int fd, int events, void* userPtr) {
    if (events == 0)
        return Unregister(fd, 0);

    if (Epoll::ModSocket(multiplexer_, fd, events, userPtr))
        return  true;

    return  errno == ENOENT && Register(fd, events, userPtr);
}


int Epoller::Poll(size_t maxEvent, int timeoutMs) {
    if (maxEvent == 0)
        return 0;

    while (events_.size() < maxEvent)
        events_.resize(2 * events_.size() + 1);

    int nFired = TEMP_FAILURE_RETRY(::epoll_wait(multiplexer_, &events_[0], maxEvent, timeoutMs));
    if (nFired == -1 && errno != EINTR && errno != EWOULDBLOCK)
        return -1;

    auto& events = firedEvents_;
    if (nFired > 0)
        events.resize(nFired);

    for (int i = 0; i < nFired; ++ i) {
        FiredEvent& fired = events[i];
        fired.events   = 0;
        fired.userdata = events_[i].data.ptr;

        if (events_[i].events & EPOLLIN)
            fired.events  |= eET_Read;

        if (events_[i].events & EPOLLOUT)
            fired.events  |= eET_Write;

        if (events_[i].events & (EPOLLERR | EPOLLHUP))
            fired.events  |= eET_Error;
    }

    return nFired;
}

} // end namespace internal
} // end namespace ananas

#else

void __Dummy__() {
}

#endif

