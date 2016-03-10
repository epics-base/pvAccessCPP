/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef CASTATUS_H
#define CASTATUS_H

#include <string>

#include <shareLib.h>

namespace epics {
namespace pvAccess {
namespace ca {

enum AlarmStatus {
    noStatus,deviceStatus,driverStatus,recordStatus,
    dbStatus,confStatus,undefinedStatus,clientStatus
};

epicsShareExtern std::string dbrStatus2alarmMessage[];
epicsShareExtern int dbrStatus2alarmStatus[];

}
}
}

#endif  /* CASTATUS_H */
