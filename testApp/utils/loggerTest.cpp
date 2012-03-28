/*
 * loggerTest.cpp
 *
 *  Created on: Dec 10, 2010
 *      Author: Miha Vitorovic
 */

#include <pv/logger.h>

#include <epicsExit.h>

#include <iostream>

using namespace epics::pvAccess;
using std::cout;

int main(int argc, char *argv[]) {

    createFileLogger("loggerTest.log");

    SET_LOG_LEVEL(logLevelDebug);
    LOG( logLevelInfo, "This will not appear");
    LOG( logLevelError, "This is a test %d", 42);
    LOG( logLevelFatal, "This is another test %f", 3.14);

    epicsExit(0);
}
