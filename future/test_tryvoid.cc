#include <iostream>
#include "Try.h"

int f() {
    return 1;
}

template <typename T, typename ...Args>
int test(Try<T> t) {
    return f(t.template Get<Args>()...);
}

int main() {
    Try<int> t(1);
    std::cout << test<int>(t) << std::endl;

    return 0;
}

