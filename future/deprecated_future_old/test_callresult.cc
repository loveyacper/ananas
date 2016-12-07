#include <iostream>
#include <cassert>
#include <typeinfo>
#include "Helper.h"

using std::cerr;
using std::endl;

long f_void(void) {
    return 1;
}

int f_int(int a) {
    return a++;
}

void f_lvalue_int(int& a) {
    ++a;
}

void f_rvalue_int(int&& a) {
    ++a;
}

int main() {
    cerr << "--------------------\n";
    cerr << typeid(CallableResult<decltype(f_void), int>::Arg::Type).name() << endl; // long 
    cerr << typeid(CallableResult<decltype(f_int), int>::Arg::Type).name() << endl; // int
    cerr << "--------------------\n";
    auto lbd = []()-> int {
        return 0;
    };
    cerr << typeid(CallableResult<decltype(lbd), void>::Arg::Type).name()<< endl; //error:  can not compile
    cerr << typeid(CallableResult<decltype(lbd), int>::Arg::Type).name()<< endl; // 
}

