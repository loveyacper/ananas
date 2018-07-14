#if defined(__APPLE__)

#include "Kqueue.h"
#include "AnanasDebug.h"

#include <errno.h>
#include <unistd.h>

namespace ananas
{
namespace internal
{

Kqueue::Kqueue()
{
    multiplexer_ = ::kqueue();
    ANANAS_INF << "create kqueue:  " << multiplexer_;
}

Kqueue::~Kqueue()
{
    ANANAS_INF << "close kqueue: " << multiplexer_;
    if (multiplexer_ != -1)  
        ::close(multiplexer_);
}

bool Kqueue::Register(int sock, int events, void* userPtr)
{
     struct kevent change[2];
         
     int  cnt = 0;
     
     if (events & eET_Read)
     {
         EV_SET(change + cnt, sock, EVFILT_READ, EV_ADD, 0, 0, userPtr);
         ++ cnt;
     }
                 
     if (events & eET_Write)
     {
         EV_SET(change + cnt, sock, EVFILT_WRITE, EV_ADD, 0, 0, userPtr);
         ++ cnt;
     }
                     
     return kevent(multiplexer_, change, cnt, NULL, 0, NULL) != -1;
}
    
bool Kqueue::Unregister(int sock, int events)
{
    struct kevent change[2];
    int cnt = 0;

    if (events & eET_Read)
    {
        EV_SET(change + cnt, sock, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        ++ cnt;
    }

    if (events & eET_Write)
    {
        EV_SET(change + cnt, sock, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        ++ cnt;
    }
                    
    if (cnt == 0)
        return false;
                        
    return -1 != kevent(multiplexer_, change, cnt, NULL, 0, NULL);
}

   
bool Kqueue::Modify(int sock, int events, void* userPtr)
{
    bool ret = Unregister(sock, eET_Read | eET_Write);
    if (events == 0)
        return ret;

    return Register(sock, events, userPtr);
}


int Kqueue::Poll(std::size_t maxEvent, int timeoutMs)
{
    if (maxEvent == 0)
        return 0;

    while (events_.size() < maxEvent)
        events_.resize(2 * events_.size() + 1);

    struct timespec* pTimeout = NULL;  
    struct timespec  timeout;
    if (timeoutMs >= 0)
    {
        pTimeout = &timeout;
        timeout.tv_sec  = timeoutMs / 1000;
        timeout.tv_nsec = timeoutMs % 1000 * 1000000;
    }

    int nFired = ::kevent(multiplexer_, NULL, 0, &events_[0], static_cast<int>(maxEvent), pTimeout);
    if (nFired == -1)
        return -1;

    auto& events = firedEvents_;
    if (nFired > 0 && static_cast<size_t>(nFired) > events.size())
        events.resize(nFired);

    for (int i = 0; i < nFired; ++ i)
    {
        FiredEvent& fired = events[i];
        fired.events   = 0;
        fired.userdata = events_[i].udata;

        if (events_[i].filter == EVFILT_READ)
            fired.events  |= eET_Read;

        if (events_[i].filter == EVFILT_WRITE)
            fired.events  |= eET_Write;

        if (events_[i].flags & EV_ERROR)
            fired.events  |= eET_Error;
    }

    return nFired;
}

} // end namespace internal
} // end namespace ananas

#else

void __Dummy__()
{
}

#endif

