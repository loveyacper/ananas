#include <iostream>
#include <cassert>
#include <typeinfo>
#include "future/Helper.h"

using std::cerr;
using std::endl;

using namespace ananas::internal;

long f_void(void) { return 1; }

int f_int(int a) { return a++; }

void f_lvalue_int(int& a) { ++a; }

void f_rvalue_int(int&& a) { ++a; }

int main()
{
    cerr << "--------------------\n";
    cerr << typeid(CallableResult<decltype(f_void), int>::Arg::Type).name() << endl; // long 
    cerr << typeid(CallableResult<decltype(f_int), int>::Arg::Type).name() << endl; // int
    cerr << "--------------------\n";
    auto lbd = []()-> int {
        return 0;
    };

    cerr << typeid(CallableResult<decltype(lbd), void>::Arg::Type).name()<< endl;
    cerr << typeid(CallableResult<decltype(lbd), int>::Arg::Type).name()<< endl;

    cerr << "Except [true false]\n";
    auto vlbd = []() {};
    cerr << CanCallWith<decltype(vlbd)>::value << endl; // true
    cerr << CanCallWith<decltype(vlbd), int>::value << endl; // false

    cerr << "Except [true false]\n";
    cerr << CanCallWith<decltype(f_void)>::value << endl; // true
    cerr << CanCallWith<decltype(f_void), int>::value << endl; // false

    cerr << "Except [true true true false]\n";
    cerr << CanCallWith<decltype(f_int), int>::value << endl; // true
    cerr << CanCallWith<decltype(f_int), int&>::value << endl; // true
    cerr << CanCallWith<decltype(f_int), int&&>::value << endl; // true
    cerr << CanCallWith<decltype(f_int)>::value << endl; // false

    cerr << "Except [false true false false]\n";
    cerr << CanCallWith<decltype(f_lvalue_int), int>::value << endl; // false
    cerr << CanCallWith<decltype(f_lvalue_int), int&>::value << endl; // true
    cerr << CanCallWith<decltype(f_lvalue_int), int&&>::value << endl; // false
    cerr << CanCallWith<decltype(f_lvalue_int)>::value << endl; // false

    cerr << "Except [true false false true]\n";
    cerr << CanCallWith<decltype(f_rvalue_int), int>::value << endl; // true
    cerr << CanCallWith<decltype(f_rvalue_int)>::value << endl; // false
    cerr << CanCallWith<decltype(f_rvalue_int), int&>::value << endl; // false
    cerr << CanCallWith<decltype(f_rvalue_int), int&&>::value << endl; // true

    return 0;
}

