#include <thread>
#include <iostream>
#include "future/Future.h"

using namespace ananas;

template <typename Type>
void ThreadFunc(Promise<Type>& pm)
{
    std::cout << "SetValue 10\n";
    Type v = 10;
    pm.SetValue(v);
}

void ThreadFuncV(Promise<void>& pm)
{
    std::cout << "SetValue void\n";
    pm.SetValue();
}

int main()
{
    Promise<int> pm1;
    std::thread t1(ThreadFunc<int>, std::ref(pm1));

    Promise<void> pm2;
    std::thread t2(ThreadFuncV, std::ref(pm2));

    auto fall = WhenAll(pm1.GetFuture(), pm2.GetFuture());
    //std::this_thread::sleep_for(std::chrono::milliseconds(1));
    fall.Then([](std::tuple<Try<int>, Try<void>>& results) {
            std::cerr << "Then collet all! goodbye!\n";
            std::cerr << std::get<0>(results) << std::endl;
         });

    t1.join();
    t2.join();

    return 0;
}

