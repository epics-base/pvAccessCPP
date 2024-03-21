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
    const std::string testString = "  a123 , b123 ,  c123     , d123,e123  ";
    std::vector<std::string> v = split(testString, ',');
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

} // namespace

MAIN(testStringUtility)
{
    testPlan(3+6+1+1);
    testDiag("Tests for string utilities");

    test_trim();
    test_split();
    test_toLowerCase();
    test_toUpperCase();
    return testDone();
}
