#include "util/ThreadLocalSingleton.h"
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>

class Test
{
public:
    Test() : id_(id ++ )
    {
        std::cout << __FUNCTION__ << " = " << id_ << std::endl;
    }
    ~Test()
    {
        std::cout << __FUNCTION__ << " = " << id_ << std::endl;
    }

    void Print() const 
    {
        std::cout << std::this_thread::get_id() << " = " << id_ << std::endl;
    }

    Test(const Test&) = delete;
    void operator= (const Test&) = delete;

    int id_;
    static std::atomic<int> id;
};
std::atomic<int> Test::id { 0 };

class Foo
{
public:
    Foo() : id_(id ++ )
    {
        std::cout << __FUNCTION__ << " = " << id_ << std::endl;
    }
    ~Foo()
    {
        std::cout << __FUNCTION__ << " = " << id_ << std::endl;
    }

    void Print() const 
    {
        std::cout << std::this_thread::get_id() << " = " << id_ << std::endl;
    }

    Foo(const Test&) = delete;
    void operator= (const Foo&) = delete;

    int id_;
    static std::atomic<int> id;
};

std::atomic<int> Foo::id { 0 };


ananas::ThreadLocalSingleton<Test>  g_t;
//ananas::ThreadLocalSingleton<Test>  g_t2; raise exception
ananas::ThreadLocalSingleton<Foo>  g_f;

void ThreadFunc(int i)
{
    if (i % 2 == 0)
    {
        Test& t = g_t.Instance();
        t.Print();
    }
    else
    {
        Foo& f = g_f.Instance();
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
    int nThreads = 10;
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

