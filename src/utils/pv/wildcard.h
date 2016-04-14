/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

/*******************************************************************
 * Implementation of glob-style wildcard pattern matching.
 * Supported wild-card characters: '*', '?'
 */

#ifndef WILDCARD_H
#define WILDCARD_H

#include <shareLib.h>

namespace epics {
namespace pvAccess {

/**
 * Class which implements wildcard patterns and checks to see
 * if a string matches a given pattern.
 */
class epicsShareClass Wildcard
{

public:

    /**
     * This function implements wildcard pattern matching.
     * @param wildcard Wildcard pattern to be used.
     * @param test Value to test against the wildcard.
     * @return 0 if wildcard does not match test. 1 - if wildcard
     * matches test.
     */
    static int wildcardfit (const char *wildcard, const char *test);
};

}
}


#endif

