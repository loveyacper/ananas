#include <cassert>
#include <iostream>
#include <string>
#include "coroutine/Coroutine.h"


using std::cerr;
using std::endl; // for test


// A coroutine: simply twice the input integer and return
int Double(int input)
{
    cerr << "Coroutine Double: got input "
         << input
         << endl;

    cerr << "Coroutine Double: Return to Main." << endl;
    auto rsp = ananas::CoroutineMgr::Yield(std::make_shared<std::string>("I am calculating, please wait...")); 

    cerr << "Coroutine Double is resumed from Main\n"; 

    cerr<< "Coroutine Double: got message \"" 
        << *std::static_pointer_cast<std::string>(rsp) << "\""
        << endl; 

    cerr << "Exit " << __FUNCTION__ << endl;

    // twice the input
    return input * 2;
}



int main()
{
    //0. define CoroutineMgr object for each thread.  
    ananas::CoroutineMgr mgr; 

    const int input = 42;

    //1. create coroutine
    ananas::CoroutinePtr  crt(mgr.CreateCoroutine(Double, input));
        
    //2. start crt, get result from crt
    auto ret = mgr.Send(crt);
    cerr << "Main func: got reply message \"" << *std::static_pointer_cast<std::string>(ret).get() << "\""<< endl;

    //3. got the final result: 84
    auto finalResult = mgr.Send(crt, std::make_shared<std::string>("Please be quick, I am waiting for your result"));
    cerr << "Main func: got the twice of " << input << ", answer is " << *std::static_pointer_cast<int>(finalResult) << endl;
        
    cerr << "BYE BYE\n";
}

