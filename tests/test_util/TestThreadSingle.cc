#include <thread>
#include <vector>
#include <atomic>
#include <iostream>

#include "util/ThreadLocalSingleton.h"

using namespace ananas;

static std::mutex s_coutMutex;

class Test : public ThreadLocalSingleton<Test>
{
private:
    DECLARE_THREAD_SINGLETON(Test);

    Test() : id_(id ++ )
    {
        std::unique_lock<std::mutex> guard(s_coutMutex);
        std::cout << __FUNCTION__ << " = " << id_ << std::endl;
    }

public:
    ~Test()
    {
        std::unique_lock<std::mutex> guard(s_coutMutex);
        std::cout << __FUNCTION__ << " = " << id_ << std::endl;
    }

    void Print() const 
    {
        std::unique_lock<std::mutex> guard(s_coutMutex);
        std::cout << std::this_thread::get_id() << " = " << id_ << std::endl;
    }

    Test(const Test&) = delete;
    void operator= (const Test&) = delete;

    int id_;
    static std::atomic<int> id;
};
std::atomic<int> Test::id { 0 };


class Foo : public ThreadLocalSingleton<Foo>
{
private:
    DECLARE_THREAD_SINGLETON(Foo);

    Foo() : id_(id ++ )
    {
        std::unique_lock<std::mutex> guard(s_coutMutex);
        std::cout << __FUNCTION__ << " = " << id_ << std::endl;
    }

public:
    ~Foo()
    {
        std::unique_lock<std::mutex> guard(s_coutMutex);
        std::cout << __FUNCTION__ << " = " << id_ << std::endl;
    }

    void Print() const 
    {
        std::unique_lock<std::mutex> guard(s_coutMutex);
        std::cout << std::this_thread::get_id() << " = " << id_ << std::endl;
    }

    Foo(const Test&) = delete;
    void operator= (const Foo&) = delete;

    int id_;
    static std::atomic<int> id;
};

std::atomic<int> Foo::id { 0 };



void ThreadFunc(int i)
{
    if (i % 2 == 0)
    {
        Test& t = Test::ThreadInstance();
        t.Print();
    }
    else
    {
        Foo& f = Foo::ThreadInstance();
        f.Print();
    }
}

void SanityCheck(int& threads)
{
    if (threads < 2)
        threads = 2;
    else if (threads > 40)
        threads = 40;
}

int main(int ac, char* av[])
{
    int nThreads = 4;
    if (ac > 1)
        nThreads = std::stoi(av[1]);

    SanityCheck(nThreads);

    std::vector<std::thread> threads;
    for (int i = 0; i < nThreads; ++ i)
    {
        threads.push_back(std::thread(ThreadFunc, i));
    }

    for (auto& t : threads)
        t.join();
}

