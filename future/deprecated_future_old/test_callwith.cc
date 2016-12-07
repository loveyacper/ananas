#include <iostream>
#include <cassert>
#include "Helper.h"

using std::cerr;
using std::endl;

int f_void(void) {
    return 0;
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
    cerr << "Except [true false]\n";
    auto lbd = []() {};
    cerr << CanCallWith<decltype(lbd)>::value << endl; // true
    cerr << CanCallWith<decltype(lbd), int>::value << endl; // false

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
}

