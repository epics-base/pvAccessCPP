#include <gtest/gtest.h>

#include <pv/wildcharMatcher.h>

using namespace epics::pvAccess;

TEST(wildcharMatcher, testSet)
{
    EXPECT_TRUE (testSet("[abc]", 1, 'a'));
    EXPECT_TRUE (testSet("[abc]", 1, 'b'));
    EXPECT_TRUE (testSet("[abc]", 1, 'c'));
    EXPECT_FALSE(testSet("[abc]", 1, 'd'));
    EXPECT_TRUE (testSet("[!abc]", 1, 'd'));
    EXPECT_FALSE(testSet("[a-c]", 1, 'd'));
    EXPECT_TRUE (testSet("[!a-c]", 1, 'd'));
    EXPECT_TRUE (testSet("[ac-f]", 1, 'd'));
    EXPECT_FALSE(testSet("[!ac-f]", 1, 'd'));
}

TEST(wildcharMatcher, testMatch)
{
    epics::pvData::String testString = "Test string for matcher";
    
    EXPECT_TRUE (match("*", testString));
    EXPECT_FALSE(match("test*", testString));
    EXPECT_TRUE (match("*est*", testString));
    EXPECT_TRUE (match("?est*", testString));
    EXPECT_FALSE(match("??est*", testString));
    EXPECT_TRUE (match("*string*", testString));
    EXPECT_FALSE(match("*[abc]tring*", testString));
    EXPECT_TRUE (match("*[!abc]tring*", testString));
    EXPECT_TRUE (match("*[p-z]tring*", testString));
}
