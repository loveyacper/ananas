
#include "gtest/gtest.h"
#include "util/Delegate.h"

void Inc(int& b) {
    ++ b;
}

TEST(delegate, c_func) {
    int n = 0;
    ananas::Delegate<void (int& )> cb;

    cb += Inc;
    cb(n);
    EXPECT_TRUE(n == 1);
}

class Context {
public:
    void MInc(int& b) {
        ++ b;
    }
};

TEST(delegate, mem_func) {
    Context t;
    int n = 0;
    ananas::Delegate<void (int& )> cb;

    cb += std::bind(&Context::MInc, &t, std::placeholders::_1);
    cb(n);
    cb(n);
    EXPECT_TRUE(n == 2);
}


TEST(delegate, lambda) {
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

TEST(delegate, copy_f) {
    int n = 0;
    ananas::Delegate<void (int )> cb;

    cb += IncCopy;
    cb(n);
    EXPECT_TRUE(n == 0);
}

