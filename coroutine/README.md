# Coroutine
=========

A pythonic coroutine implementation in C++11.

## Requirements
* C++11
* Linux or Windows

## Principle
* linux: Use `swapcontext`, please `man makecontext`.
* windows: the fiber API. See [MSDN](https://msdn.microsoft.com/en-us/library/windows/desktop/ms682661(v=vs.85).aspx) for details.

## Code example
```c++

// A coroutine: simply twice the input integer and return
int Double(int input)
{
    cerr << "Coroutine Double: got input "
         << input
         << endl;

    cerr << "Coroutine Double: Return to Main." << endl;
    auto rsp = Coroutine::Yield(std::make_shared<std::string>("I am calculating, please wait...")); 

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
    const int input = 42;

    //1. create coroutine
    CoroutinePtr crt(Coroutine::CreateCoroutine(Double, input));
        
    //2. start crt, get result from crt
    auto ret = mgr.Send(crt);
    cerr << "Main func: got reply message \"" << *std::static_pointer_cast<std::string>(ret).get() << "\""<< endl;

    //3. got the final result: 84
    auto finalResult = mgr.Send(crt, std::make_shared<std::string>("Please be quick, I am waiting for your result"));
    cerr << "Main func: got the twice of " << input << ", answer is " << *std::static_pointer_cast<int>(finalResult) << endl;
        
    cerr << "BYE BYE\n";
}

```

