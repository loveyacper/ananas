
#include "gtest/gtest.h"
#include "util/Buffer.h"

using namespace ananas;


TEST(buffer, push) {
    Buffer buf;

    size_t ret = buf.PushData("hello", 5);
    EXPECT_EQ(ret, 5);

    ret = buf.PushData("world\n", 6);
    EXPECT_EQ(ret, 6);
}


TEST(buffer, peek) {
    Buffer buf;

    {
        buf.PushData("hello ", 6);
        buf.PushData("world\n", 6);
    }

    char tmp[12];
    size_t ret = buf.PeekDataAt(tmp, 5, 0);
    EXPECT_EQ(ret, 5);
    EXPECT_EQ(tmp[0], 'h');
    EXPECT_EQ(tmp[4], 'o');

    ret = buf.PeekDataAt(tmp, 2, 6);
    EXPECT_EQ(ret, 2);
    EXPECT_EQ(tmp[0], 'w');
    EXPECT_EQ(tmp[1], 'o');

    EXPECT_EQ(buf.ReadableSize(), 12);
}

TEST(buffer, pop) {
    Buffer buf;

    {
        buf.PushData("hello ", 6);
        buf.PushData("world\n", 6);
    }

    size_t cap = buf.Capacity();

    char tmp[12];
    size_t ret = buf.PopData(tmp, 6);
    EXPECT_EQ(ret, 6);
    EXPECT_EQ(tmp[0], 'h');
    EXPECT_EQ(tmp[5], ' ');

    EXPECT_EQ(buf.ReadableSize(), 6);

    ret = buf.PopData(tmp, 6);
    EXPECT_EQ(ret, 6);
    EXPECT_EQ(tmp[0], 'w');
    EXPECT_EQ(tmp[5], '\n');

    EXPECT_TRUE(buf.IsEmpty());
    EXPECT_EQ(buf.Capacity(), cap); // pop does not change capacity
}

TEST(buffer, shrink) {
    Buffer buf;

    {
        buf.PushData("hello ", 6);
        buf.PushData("world\n", 6);
    }

    EXPECT_NE(buf.Capacity(), 12);

    buf.Shrink();
    EXPECT_EQ(buf.Capacity(), 16);

    buf.PushData("abcd", 4);
    EXPECT_EQ(buf.Capacity(), 16);

    char tmp[16];
    buf.PopData(tmp, sizeof tmp);

    EXPECT_EQ(buf.Capacity(), 16);
}

TEST(buffer, push_pop) {
    Buffer buf;

    buf.PushData("hello ", 6);

    char tmp[8];
    size_t ret = buf.PopData(tmp, 5);

    EXPECT_EQ(ret, 5);
    EXPECT_EQ(buf.Capacity(), Buffer::kDefaultSize);

    buf.Shrink();
    EXPECT_EQ(buf.Capacity(), 1);
}

