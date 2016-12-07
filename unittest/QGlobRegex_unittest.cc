#include "UnitTest.h"
#include "QGlobRegex.h"

using namespace qedis;

TEST_CASE(regex_star)
{
    EXPECT_TRUE(glob_match("a*b?*?", "abxy"));
    EXPECT_FALSE(glob_match("a*b?*?", "cb"));

    EXPECT_TRUE(glob_match("a*b", "ab"));
    EXPECT_TRUE(glob_match("a*b", "acb"));
    EXPECT_TRUE(glob_match("a*b", "acccb"));
    EXPECT_FALSE(glob_match("a*b", "accc"));
}

TEST_CASE(regex_question)
{
    EXPECT_FALSE(glob_match("a?b", "ab"));
    EXPECT_TRUE(glob_match("a?b", "acb"));
    EXPECT_FALSE(glob_match("a?b", "acc"));
    EXPECT_FALSE(glob_match("a?b", "accb"));
}

TEST_CASE(regex_bracket)
{
    EXPECT_TRUE(glob_match("[-]", "-"));
    EXPECT_TRUE(glob_match("[-abc]", "-"));
    EXPECT_TRUE(glob_match("[-]??", "--x"));

    EXPECT_TRUE(glob_match("[a-ce-a]", "a"));

    EXPECT_TRUE(glob_match("[a-eg-z]", "u"));
    EXPECT_FALSE(glob_match("[^a-eg-z]", "u"));
    EXPECT_TRUE(glob_match("[abcx-z]", "c"));
    EXPECT_TRUE(glob_match("[abcx-z]", "y"));
    EXPECT_FALSE(glob_match("[^abcx-z]", "c"));
    EXPECT_FALSE(glob_match("[^abcx-z]", "y"));

    EXPECT_FALSE(glob_match("[a-z]", "U"));
    EXPECT_TRUE(glob_match("[^a-z]", "U"));

    EXPECT_TRUE(glob_match("[a--]", "-"));
    EXPECT_FALSE(glob_match("[^a--]", "-"));

    EXPECT_TRUE(glob_match("[---]", "-"));
    EXPECT_TRUE(glob_match("[-]", "-"));
    EXPECT_TRUE(glob_match("[-xyz]", "-"));
    EXPECT_TRUE(glob_match("[]]", "]"));
    EXPECT_TRUE(glob_match("x[]abc]", "x]"));
    EXPECT_FALSE(glob_match("x[ab]c]", "x]"));

    EXPECT_TRUE(glob_match("x\\[ab]y", "x[ab]y"));
    EXPECT_TRUE(glob_match("x*\\", "xab\\"));
    EXPECT_TRUE(glob_match("\\*\\", "\\fuckyou\\"));
}

TEST_CASE(regex_star_brackets)
{
    EXPECT_FALSE(glob_match("[a-e]", "xa"));
    EXPECT_FALSE(glob_match("*[a-e]*", "x"));
    EXPECT_FALSE(glob_match("*[a-e]*", "xy"));
    EXPECT_TRUE(glob_match("*[a-e]*", "xya"));
    EXPECT_TRUE(glob_match("*[a-e]*", "xay"));
    EXPECT_TRUE(glob_match("*[a-e]*", "axy"));
}

TEST_CASE(regex_search)
{
    EXPECT_TRUE(glob_search("[a-e]", "xa"));
    EXPECT_FALSE(glob_search("[a-e]", "x"));
    EXPECT_FALSE(glob_search("[a-e]", "xy"));
    EXPECT_TRUE(glob_search("[a-e]", "xya"));
    EXPECT_TRUE(glob_search("[a-e]", "xay"));
    EXPECT_TRUE(glob_search("[a-e]", "axy"));
}

TEST_CASE(regex_strange)
{
    EXPECT_FALSE(glob_match("[a-e", "a"));
    EXPECT_TRUE(glob_match("*a-e]*", "a-e]"));
    EXPECT_FALSE(glob_match("[a-e*", "a"));
    EXPECT_TRUE(glob_match("a[-]", "a-"));
    EXPECT_FALSE(glob_match("a\[", "a["));
    EXPECT_TRUE(glob_match("a\\[", "a["));
    EXPECT_TRUE(glob_match("a[]]", "a]"));

    EXPECT_TRUE(glob_match("a[^^]", "a$"));
    EXPECT_FALSE(glob_match("a[^^]", "a^"));

    EXPECT_TRUE(glob_match("a**[^^]", "a^!"));
    EXPECT_FALSE(glob_match("**a**", "xyz"));
}

