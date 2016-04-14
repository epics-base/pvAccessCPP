/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <epicsString.h>

#define epicsExportSharedSymbols
#include <pv/wildcard.h>

using namespace epics::pvAccess;

int
Wildcard::wildcardfit (const char *wildcard, const char *test)
{
    return epicsStrGlobMatch(test, wildcard);
}
