/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <fstream>
#include <iostream>
#include <time.h>
#include <cstring>
#include <stdio.h>

#include <epicsExit.h>
#include <errlog.h>
#include <epicsTime.h>

#include <pv/noDefaultMethods.h>
#include <pv/lock.h>
#include <pv/pvType.h>

#define epicsExportSharedSymbols
#include <pv/logger.h>

using namespace epics::pvData;
using std::ofstream;
using std::ios;
using std::endl;

namespace epics {
namespace pvAccess {

#define TIMETEXTLEN 32

static pvAccessLogLevel g_pvAccessLogLevel = logLevelInfo;

void pvAccessLog(pvAccessLogLevel level, const char* format, ...)
{
    // TODO lock
    if (level >= g_pvAccessLogLevel)
    {
        char timeText[TIMETEXTLEN];
        epicsTimeStamp tsNow;

        epicsTimeGetCurrent(&tsNow);
        epicsTimeToStrftime(timeText, TIMETEXTLEN, "%Y-%m-%dT%H:%M:%S.%03f", &tsNow);

        printf("%s ", timeText);

        va_list arg;
        va_start(arg, format);
        vprintf(format, arg);
        va_end(arg);

        printf("\n");
        fflush(stdout);    // needed for WIN32
    }
}

void pvAccessSetLogLevel(pvAccessLogLevel level)
{
    g_pvAccessLogLevel = level;
}

bool pvAccessIsLoggable(pvAccessLogLevel level)
{
    return level >= g_pvAccessLogLevel;
}

}
}
