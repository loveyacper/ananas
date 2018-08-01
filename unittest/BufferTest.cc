
#include "UnitTest.h"
#include "util/Buffer.h"

using namespace ananas;


TEST_CASE(push) {
    Buffer buf;

    size_t ret = buf.PushData("hello", 5);
    EXPECT_TRUE(ret == 5);

    ret = buf.PushData("world\n", 6);
    EXPECT_TRUE(ret == 6);
}


TEST_CASE(peek) {
    Buffer buf;

    {
        buf.PushData("hello ", 6);
        buf.PushData("world\n", 6);
    }

    char tmp[12];
    size_t ret = buf.PeekDataAt(tmp, 5, 0);
    EXPECT_TRUE(ret == 5);
    EXPECT_TRUE(tmp[0] == 'h');
    EXPECT_TRUE(tmp[4] == 'o');

    ret = buf.PeekDataAt(tmp, 2, 6);
    EXPECT_TRUE(ret == 2);
    EXPECT_TRUE(tmp[0] == 'w');
    EXPECT_TRUE(tmp[1] == 'o');

    EXPECT_TRUE(buf.ReadableSize() == 12);
}

TEST_CASE(pop) {
    Buffer buf;

    {
        buf.PushData("hello ", 6);
        buf.PushData("world\n", 6);
    }

    size_t cap = buf.Capacity();

    char tmp[12];
    size_t ret = buf.PopData(tmp, 6);
    EXPECT_TRUE(ret == 6);
    EXPECT_TRUE(tmp[0] == 'h');
    EXPECT_TRUE(tmp[5] == ' ');

    EXPECT_TRUE(buf.ReadableSize() == 6);

    ret = buf.PopData(tmp, 6);
    EXPECT_TRUE(ret == 6);
    EXPECT_TRUE(tmp[0] == 'w');
    EXPECT_TRUE(tmp[5] == '\n');

    EXPECT_TRUE(buf.IsEmpty());
    EXPECT_TRUE(buf.Capacity() == cap); // pop does not change capacity
}

TEST_CASE(shrink) {
    Buffer buf;

    {
        buf.PushData("hello ", 6);
        buf.PushData("world\n", 6);
    }

    EXPECT_FALSE(buf.Capacity() == 12);

    buf.Shrink();
    EXPECT_TRUE(buf.Capacity() == 16);

    buf.PushData("abcd", 4);
    EXPECT_TRUE(buf.Capacity() == 16);

    char tmp[16];
    buf.PopData(tmp, sizeof tmp);

    EXPECT_TRUE(buf.Capacity() == 16);

    buf.Shrink();
    EXPECT_TRUE(buf.Capacity() == 0);
}

TEST_CASE(push_pop) {
    Buffer buf;

    buf.PushData("hello ", 6);

    char tmp[8];
    size_t ret = buf.PopData(tmp, 5);

    EXPECT_TRUE(ret == 5);
    EXPECT_TRUE(buf.Capacity() == Buffer::kDefaultSize);

    buf.Shrink();
    EXPECT_TRUE(buf.Capacity() == 1);
}

int main() {
    RUN_ALL_TESTS();
    return 0;
}

