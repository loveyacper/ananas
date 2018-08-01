
#include "UnitTest.h"
#include "util/Delegate.h"

void Inc(int& b) {
    ++ b;
}

TEST_CASE(c_func) {
    int n = 0;
    ananas::Delegate<void (int& )> cb;

    cb += Inc;
    cb(n);
    EXPECT_TRUE(n == 1);

    cb -= Inc;
    cb(n);
    EXPECT_TRUE(n == 1);
}

class Test {
public:
    void MInc(int& b) {
        ++ b;
    }
};

TEST_CASE(mem_func) {
    Test t;
    int n = 0;
    ananas::Delegate<void (int& )> cb;

    cb += std::bind(&Test::MInc, &t, std::placeholders::_1);
    cb(n);
    cb(n);
    EXPECT_TRUE(n == 2);
}


TEST_CASE(lambda) {
    int n = 0;
    ananas::Delegate<void (int& )> cb;

    cb += [](int& b) {
        ++ b;
    };
    cb(n);
    EXPECT_TRUE(n == 1);
}

void IncCopy(int b) {
    ++ b;
}

TEST_CASE(copy_f) {
    int n = 0;
    ananas::Delegate<void (int )> cb;

    cb += IncCopy;
    cb(n);
    EXPECT_TRUE(n == 0);

    cb -= IncCopy;
    cb(n);
    EXPECT_TRUE(n == 0);
}


int main() {
    RUN_ALL_TESTS();
    return 0;
}

