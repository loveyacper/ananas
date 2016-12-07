#include <cassert>
#include <iostream>
#include <string>
#include "Coroutine.h"
#include "Future.h"


using std::cerr;
using std::endl; // for test

class Test
{
public:
    Test()
    {
        cerr << __FUNCTION__ << endl;
    }

    ~Test()
    {
        cerr << __FUNCTION__ << endl;
    }
};

// A coroutine: simply twice the input integer and return
void Double(const std::shared_ptr<void>& inParams, std::shared_ptr<void>& outParams)
{
    Test t; // TEST: destruct until finish Double;

    int input = *std::static_pointer_cast<int>(inParams);
    cerr << "Coroutine Double: got input params "
         << input
         << endl;

    cerr << "Coroutine Double: Return to Main." << endl;
    auto rsp = CoroutineMgr::Yield(std::make_shared<std::string>("I am calculating, please wait...")); 

    cerr << "Coroutine Double is resumed from Main\n"; 

    cerr<< "Coroutine Double: got message \"" 
        << *std::static_pointer_cast<std::string>(rsp) << "\""
        << endl; 

    // double the input
    outParams = std::make_shared<int>(input * 2);

    cerr << "Exit " << __FUNCTION__ << endl;
}


void FutureTest(const std::shared_ptr<void>& inParams, std::shared_ptr<void>& outParams)
{
    int input = *std::static_pointer_cast<int>(inParams);
    cerr << "Coroutine : got input params "
         << input
         << endl;

    cerr << "Coroutine : Return Main.\n";
    std::shared_ptr<void> result = std::make_shared<int>(input + 1);
    //std::shared_ptr<int> test = std::static_pointer_cast<int>(result);
    //cerr << "test " << *test << endl;
    //auto rsp = CoroutineMgr::Yield(std::make_shared<Future<std::shared_ptr<void>>>(result));
            
    using FutureAny = Future<std::shared_ptr<void> >;
    std::shared_ptr<FutureAny> ft = std::make_shared<FutureAny>();
    ft->SetValue(result);
    auto rsp = CoroutineMgr::Yield(ft);

    std::shared_ptr<int> res = std::static_pointer_cast<int>(rsp);
    cerr << "Coroutine is resumed from Main, result = " << *res << endl ; 

    // another future
    ft = std::make_shared<FutureAny>();
    {
        std::shared_ptr<int> test = std::static_pointer_cast<int>(result);
        *test = 100;
    }
    ft->SetValue(result);
    rsp = CoroutineMgr::Yield(ft);
    res = std::static_pointer_cast<int>(rsp);
    cerr << "22222Coroutine is resumed from Main, result = " << *res << endl ; 

#if 0
    cerr<< "Coroutine : got message \"" 
        << *std::static_pointer_cast<std::string>(rsp) << "\""
        << endl; 

    // double the input
    outParams = std::make_shared<int>(input * 2);
#endif

    cerr << "Exit " << __FUNCTION__ << endl;
}

int main()
{
    //0. define CoroutineMgr object for each thread.  
    //CoroutineMgr   mgr; 
    auto& mgr = CoroutineMgr::Current();

#if 0
    //1. create coroutine
    CoroutinePtr  crt(mgr.CreateCoroutine(Double));
        
    //2. jump to crt1, get result from crt1
    const int input = 42;
    auto ret = mgr.Send(crt, std::make_shared<int>(input));
    cerr << "Main func: got reply message \"" << *std::static_pointer_cast<std::string>(ret).get() << "\""<< endl;

    //3. got the final result: 84
    auto finalResult = mgr.Send(crt, std::make_shared<std::string>("Please be quick, I am waiting for your result"));
    cerr << "Main func: got the twice of " << input << ", answer is " << *std::static_pointer_cast<int>(finalResult) << endl;
        
    try 
    {
        // crt is over, this is wrong!
        mgr.Send(crt, std::make_shared<std::string>("fuck, it is error"));
    }
    catch (...)
    {
        cerr << "future is done, bye bye\n";
        return 0;
    }
#else
    RunCoroutine(FutureTest, std::make_shared<int>(9));
#endif
    cerr << "BYE BYE\n";
}

