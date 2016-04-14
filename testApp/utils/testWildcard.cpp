/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/wildcard.h>

#include <epicsUnitTest.h>
#include <testMain.h>

using epics::pvAccess::Wildcard;

static
void testWildcardCases()
{
    testDiag("Test testWildcardCases()");

    testOk1(Wildcard::wildcardfit("*", "test"));
    testOk1(Wildcard::wildcardfit("*", ""));
    testOk1(!Wildcard::wildcardfit("?", ""));
    testOk1(Wildcard::wildcardfit("?", "a"));
    testOk1(Wildcard::wildcardfit("*?", "test"));
    testOk1(Wildcard::wildcardfit("?*", "test"));
    testOk1(Wildcard::wildcardfit("?*?", "test"));
    testOk1(!Wildcard::wildcardfit("?", ""));
    testOk1(!Wildcard::wildcardfit("?*?", ""));

    //testOk1(Wildcard::wildcardfit("[-aa-]*", "01 abAZ"));
    //testOk1(Wildcard::wildcardfit("[\\!a\\-bc]*", "!!!b-bb-"));
    testOk1(Wildcard::wildcardfit("*zz", "zz"));
    //testOk1(Wildcard::wildcardfit("[abc]*zz", "zz"));

    //testOk1(!Wildcard::wildcardfit("[!abc]*a[def]", "xyzbd"));
    //testOk1(Wildcard::wildcardfit("[!abc]*a[def]", "xyzad"));
    //testOk1(Wildcard::wildcardfit("[a-g]l*i?", "gloria"));
    //testOk1(Wildcard::wildcardfit("[!abc]*e", "smile"));
    //testOk1(Wildcard::wildcardfit("[-z]", "a"));
    //testOk1(!Wildcard::wildcardfit("[]", ""));
    //testOk1(Wildcard::wildcardfit("[a-z]*", "java"));
    testOk1(Wildcard::wildcardfit("*.*", "command.com"));
    testOk1(!Wildcard::wildcardfit("*.*", "/var/etc"));
    //testOk1(Wildcard::wildcardfit("**?*x*[abh-]*Q", "XYZxabbauuZQ"));
}

MAIN(testWildcard)
{
    testPlan(12);
    testDiag("Tests for Wildcard util");

    testWildcardCases();
    return testDone();
}
