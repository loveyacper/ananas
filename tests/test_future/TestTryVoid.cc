#include <iostream>
#include "future/Try.h"

using namespace ananas;

int f() {
    return 1;
}

template <typename T, typename ...Args>
int test(Try<T> t) {
    // amazing...
    return f(t.template Get<Args>()...);
}

int main() {
    Try<void> t;
    std::cout << test<void>(t) << std::endl;

    return 0;
}

