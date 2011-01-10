/*
 * wildcharMatcher.cpp
 *
 *  Created on: Nov 8, 2010
 *      Author: Miha Vitorovic
 */

#include <wildcharMatcher.h>

#include <iostream>
#include <epicsAssert.h>

using namespace epics::pvData;
using namespace epics::pvAccess;
using std::cout;

int main(int argc, char *argv[]) {
    String testString = "Test string for matcher";

    cout<<"testSet(\"[abc]\",1,'a').\n";
    assert(testSet("[abc]", 1, 'a'));
    cout<<"testSet(\"[abc]\",1,'b').\n";
    assert(testSet("[abc]", 1, 'b'));
    cout<<"testSet(\"[abc]\",1,'c').\n";
    assert(testSet("[abc]", 1, 'c'));
    cout<<"testSet(\"[abc]\",1,'d').\n";
    assert(!testSet("[abc]", 1, 'd'));
    cout<<"testSet(\"[!abc]\",1,'d').\n";
    assert(testSet("[!abc]", 1, 'd'));
    cout<<"testSet(\"[a-c]\",1,'d').\n";
    assert(!testSet("[a-c]", 1, 'd'));
    cout<<"testSet(\"[!a-c]\",1,'d').\n";
    assert(testSet("[!a-c]", 1, 'd'));
    cout<<"testSet(\"[ac-f]\",1,'d').\n";
    assert(testSet("[ac-f]", 1, 'd'));
    cout<<"testSet(\"[!ac-f]\",1,'d').\n";
    assert(!testSet("[!ac-f]", 1, 'd'));


    cout<<"\n";

    cout<<"Test string is: \""<<testString<<"\"\n";

    cout<<"Testing for '*'\n";
    assert(match("*", testString));
    cout<<"Testing for 'test*'.\n";
    assert(!match("test*", testString));
    cout<<"Testing for '*est*'.\n";
    assert(match("*est*", testString));
    cout<<"Testing for '?est*'.\n";
    assert(match("?est*", testString));
    cout<<"Testing for '??est*'.\n";
    assert(!match("??est*", testString));
    cout<<"Testing for '*string*'.\n";
    assert(match("*string*", testString));
    cout<<"Testing for '*[abc]tring*'.\n";
    assert(!match("*[abc]tring*", testString));
    cout<<"Testing for '*[!abc]tring*'.\n";
    assert(match("*[!abc]tring*", testString));
    cout<<"Testing for '*[p-z]tring*'.\n";
    assert(match("*[p-z]tring*", testString));
    cout<<"\nPASSED!\n";

}
