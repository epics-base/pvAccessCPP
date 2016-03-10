/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define epicsExportSharedSymbols

#include <pv/caStatus.h>

namespace epics {
namespace pvAccess {
namespace ca {

std::string dbrStatus2alarmMessage[] = {
    "NO_ALARM",     // 0 ..
    "READ_ALARM",
    "WRITE_ALARM",
    "HIHI_ALARM",
    "HIGH_ALARM",
    "LOLO_ALARM",
    "LOW_ALARM",
    "STATE_ALARM",
    "COS_ALARM",
    "COMM_ALARM",
    "TIMEOUT_ALARM",
    "HW_LIMIT_ALARM",
    "CALC_ALARM",
    "SCAN_ALARM",
    "LINK_ALARM",
    "SOFT_ALARM",
    "BAD_SUB_ALARM",
    "UDF_ALARM",
    "DISABLE_ALARM",
    "SIMM_ALARM",
    "READ_ACCESS_ALARM",
    "WRITE_ACCESS_ALARM"        // .. 21
};

int dbrStatus2alarmStatus[] = {
    noStatus,           //"NO_ALARM"
    driverStatus,       //"READ_ALARM",
    driverStatus,       //"WRITE_ALARM",
    recordStatus,       //"HIHI_ALARM",
    recordStatus,       //"HIGH_ALARM",
    recordStatus,       //"LOLO_ALARM",
    recordStatus,       //"LOW_ALARM",
    recordStatus,       //"STATE_ALARM",
    recordStatus,       //"COS_ALARM",
    driverStatus,       //"COMM_ALARM",
    driverStatus,       //"TIMEOUT_ALARM",
    deviceStatus,       //"HW_LIMIT_ALARM",
    recordStatus,       //"CALC_ALARM",
    dbStatus,           //"SCAN_ALARM",
    dbStatus,           //"LINK_ALARM",
    dbStatus,           //"SOFT_ALARM",
    confStatus,         //"BAD_SUB_ALARM",
    recordStatus,       //"UDF_ALARM",
    recordStatus,       //"DISABLE_ALARM",
    recordStatus,       //"SIMM_ALARM",
    clientStatus,       //"READ_ACCESS_ALARM",
    clientStatus        //"WRITE_ACCESS_ALARM"        // .. 21
};


}
}
}


