/*
 * logger.h
 *
 *  Created on: Dec 10, 2010
 *      Author: Miha Vitorovic
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#include <pv/pvType.h>

#include <errlog.h>
 
namespace epics {
    namespace pvAccess {
        
        typedef enum { logLevelAll = 0, logLevelTrace, logLevelDebug, logLevelInfo,
                                logLevelWarn, logLevelError, logLevelFatal, logLevelOff } pvAccessLogLevel;
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


        void pvAccessLog(pvAccessLogLevel level, const char* format, ...);
        void pvAccessSetLogLevel(pvAccessLogLevel level);

        #define LOG(level, format, ...) pvAccessLog(level, format, ##__VA_ARGS__)
        #define SET_LOG_LEVEL(level) pvAccessSetLogLevel(level)
        
        // EPICS errlog
        //#define LOG errlogSevPrintf 
        //#define SET_LOG_LEVEL(level) errlogSetSevToLog(level)
        
        // none
        //#define LOG(level, fmt, ...)
        //#define SET_LOG_LEVEL(level)
        
        /**
         * Create a logger that will write to file indicated by the <tt>fname</tt>.
         * After creation you are free to use standard EPICSv3 functions from
         * <tt>errlog.h</tt>.
         *
         * @param[in] fname The file to write to. If the file exists, it
         * is opened for append.
         */
        void createFileLogger( epics::pvData::String fname );

    }
}

#endif /* LOGGER_H_ */
