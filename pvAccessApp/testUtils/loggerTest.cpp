/*
 * loggerTest.cpp
 *
 *  Created on: Dec 10, 2010
 *      Author: Miha Vitorovic
 */

#include "logger.h"

#include <errlog.h>
#include <epicsExit.h>

#include <iostream>

using namespace epics::pvAccess;
using std::cout;

int main(int argc, char *argv[]) {

    createFileLogger("loggerTest.log");

    errlogSetSevToLog(errlogMinor);
    errlogSevPrintf( errlogInfo, "This will not appear");
    errlogSevPrintf( errlogMajor, "This is a test %d", 42);
    errlogSevPrintf( errlogFatal, "This is another test %f", 3.14);

    epicsExit(0);
}
