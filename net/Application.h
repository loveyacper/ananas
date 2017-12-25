
#ifndef BERT_APPLICATION_H
#define BERT_APPLICATION_H

#include <signal.h>
#include <atomic>
#include <vector>

namespace ananas
{

class EventLoopGroup;
    
class Application
{
    friend class EventLoopGroup;
public:
    static Application& Instance();
    ~Application();

    void Run();
    void Exit();
    bool IsExit() const;

private:
    Application();
    void _AddGroup(EventLoopGroup* group);

    std::vector<EventLoopGroup* > groups_;

    enum class State
    {
        eS_None,
        eS_Started,
        eS_Stopped,
    };
    std::atomic<State> state_;
};

} // end namespace ananas

#endif

