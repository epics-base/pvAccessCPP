#include <epicsUnitTest.h>
#include <testMain.h>

#include <pv/pvUnitTest.h>

#include <pv/stringUtility.h>

#include <epicsAssert.h>

#include <iostream>
#include <vector>

using namespace epics::pvAccess::StringUtility;

namespace {

void test_trim()
{
    testDiag("Test trim()");
    testOk1(trim("       abc123     ") == "abc123");
    testOk1(trim("abc123     ") == "abc123");
    testOk1(trim("    abc123") == "abc123");
}

void test_split()
{
    testDiag("Test split()");
    std::string testString = "  a123 , b123 ,  c123     , d123,e123  ";
    testDiag("Splitting by ',', input string: '%s'", testString.c_str());
    std::vector<std::string> v = split(testString, ',');
    testOk1(v.size() == 5);
    testOk1(v[0] == "a123");
    testOk1(v[1] == "b123");
    testOk1(v[2] == "c123");
    testOk1(v[3] == "d123");
    testOk1(v[4] == "e123");
    testString = "  a123   b123    c123       d123 e123  ";
    testDiag("Splitting by ' ' and ignoring empty tokens, input string: '%s'", testString.c_str());
    bool ignoreEmptyTokens = true;
    v = split(testString, ' ', ignoreEmptyTokens);
    testOk1(v.size() == 5);
    testOk1(v[0] == "a123");
    testOk1(v[1] == "b123");
    testOk1(v[2] == "c123");
    testOk1(v[3] == "d123");
    testOk1(v[4] == "e123");
}

void test_toLowerCase()
{
    testDiag("Test toLowerCase()");
    testOk1(toLowerCase("AbCdEfGhIj12345Kl") == "abcdefghij12345kl");
}

void test_toUpperCase()
{
    testDiag("Test toUpperCase()");
    testOk1(toUpperCase("AbCdEfGhIj12345Kl") == "ABCDEFGHIJ12345KL");
}

void test_replace()
{
    testDiag("Test replace()");
    testOk1(replace("a,b,c,d,e,f,1,2,3", ",", " ") == "a b c d e f 1 2 3");
    testOk1(replace("a,b,c", ',', ' ') == "a b c");
    testOk1(replace("a,b,c", ',', "aa") == "aaabaac");
    testOk1(replace("a,b,c", ",", ",X,") == "a,X,b,X,c");
}

} // namespace

MAIN(testStringUtility)
{
    testPlan(3+12+1+1+4);
    testDiag("Tests for string utilities");

    test_trim();
    test_split();
    test_toLowerCase();
    test_toUpperCase();
    test_replace();
    return testDone();
}
