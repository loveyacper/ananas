#include <thread>
#include <iostream>
#include "Future.h"

template <typename Type>
void ThreadFunc(Promise<Type>& pm)
{
   //std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::cout << "SetValue 10\n";
    Type v = 10;
    pm.SetValue(v);
}

void ThreadFuncV(Promise<void>& pm)
{
   //std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::cout << "SetValue void\n";
    pm.SetValue();
}

int main()
{
    Promise<int> pm;
    std::thread t(ThreadFunc<int>, std::ref(pm));

    //std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Future<int> ft(pm.GetFuture());
             
    Promise<void> pmv;
#if 1
    ft.Then([](int v) {
            std::cerr << "Then got " << v << std::endl;
            return 1.0f;
            })
    .Then([](float f) {
                std::cerr << "Then got float " << f << std::endl;
                //return -1;
                }
         ).Then([&pmv]() {
             std::cerr << "Then goodbye!\n";
             std::thread t2(ThreadFuncV, std::ref(pmv));
             t2.detach();
             return pmv.GetFuture();
             }).Then([]() {
             std::cerr << "Then REAL goodbye!\n";
                 });
#endif

//    std::cout << ft.GetValue() << std::endl;
    //ft.GetValue();

    t.join();
    return 0;
}
