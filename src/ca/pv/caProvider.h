/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef CAPROVIDER_H
#define CAPROVIDER_H

#include <shareLib.h>
#include <pv/pvAccess.h>

namespace epics {
namespace pvAccess {
class Configuration;
namespace ca {

class epicsShareClass CAClientFactory
{
public:
    static void start();
    static void stop();
};

}
}
}

#endif  /* CAPROVIDER_H */
