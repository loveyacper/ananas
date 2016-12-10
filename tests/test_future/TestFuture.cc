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
    Promise<int> pm;
    std::thread t(ThreadFunc<int>, std::ref(pm));

    Future<int> ft(pm.GetFuture());
             
    Promise<void> pmv;

    ft.Then([](int v) {
            std::cerr << "1.Then got int value " << v
                      << " and return float 1.0f." << std::endl;
            return 1.0f;
            })
            .Then([](float f) {
                    std::cerr << "2.Then got float value " << f
                              << " and return nothing." << std::endl;
                })
                .Then([&pmv]() {
                        std::cerr << "3.Then got void and return another future\n";
                        std::thread t2(ThreadFuncV, std::ref(pmv));
                        t2.detach();
                        return pmv.GetFuture();
                        })
                        .Then([]() {
                                std::cerr << "4. Then GOODBYE!\n";
                                });


    t.join();
    return 0;
}
