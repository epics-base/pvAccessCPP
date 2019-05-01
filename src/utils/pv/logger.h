/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#include <string>

#include <compilerDependencies.h>
#include <shareLib.h>

namespace epics {
namespace pvAccess {

typedef enum { logLevelAll = 0, logLevelTrace, logLevelDebug, logLevelInfo,
               logLevelWarn, logLevelError, logLevelFatal, logLevelOff
             } pvAccessLogLevel;
/*
ALL
    The ALL has the lowest possible rank and is intended to turn on all logging.
TRACE
    The TRACE Level designates finer-grained informational events than the DEBUG
DEBUG
    The DEBUG Level designates fine-grained informational events that are most useful to debug an application.
INFO
    The INFO level designates informational messages that highlight the progress of the application at coarse-grained level.
WARN
    The WARN level designates potentially harmful situations.
ERROR
    The ERROR level designates error events that might still allow the application to continue running.
FATAL
    The FATAL level designates very severe error events that will presumably lead the application to abort.
OFF
    The OFF has the highest possible rank and is intended to turn off logging.
*/


epicsShareFunc void pvAccessLog(pvAccessLogLevel level, const char* format, ...) EPICS_PRINTF_STYLE(2, 3);
epicsShareFunc void pvAccessSetLogLevel(pvAccessLogLevel level);
epicsShareFunc bool pvAccessIsLoggable(pvAccessLogLevel level);

#if defined (__GNUC__) && __GNUC__ < 3
#define LOG(level, format, ARGS...) pvAccessLog(level, format, ##ARGS)
#else
#define LOG(level, format, ...) pvAccessLog(level, format, ##__VA_ARGS__)
#endif
#define SET_LOG_LEVEL(level) pvAccessSetLogLevel(level)
#define IS_LOGGABLE(level) pvAccessIsLoggable(level)

// EPICS errlog
//#define LOG errlogSevPrintf
//#define SET_LOG_LEVEL(level) errlogSetSevToLog(level)

// none
//#define LOG(level, fmt, ...)
//#define SET_LOG_LEVEL(level)

}
}

#endif /* LOGGER_H_ */
