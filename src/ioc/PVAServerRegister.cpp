/*PVAServerRegister.cpp */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2013.07.24
 */

/* Author: Marty Kraimer */

#include <cstddef>
#include <cstdlib>
#include <cstddef>
#include <string>
#include <cstdio>
#include <memory>
#include <iostream>

#include <cantProceed.h>
#include <epicsStdio.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <epicsThread.h>
#include <iocsh.h>
#include <epicsExit.h>

#include <epicsExport.h>

#include <pv/pvAccess.h>
#include <pv/serverContext.h>

using std::cout;
using std::endl;
using namespace epics::pvData;
using namespace epics::pvAccess;

static const iocshArg startPVAServerArg0 = { "providerNames", iocshArgString };
static const iocshArg *startPVAServerArgs[] = {
    &startPVAServerArg0};

static const iocshFuncDef startPVAServerFuncDef = {
    "startPVAServer", 1, startPVAServerArgs
};
static void startPVAServer(const iocshArgBuf *args)
{
    char *names = args[0].sval;
    if(!names) {
       startPVAServer(PVACCESS_ALL_PROVIDERS,0,true,true);
    } else {
        std::string providerNames(names);
        startPVAServer(providerNames,0,true,true);
    }
}


static void registerStartPVAServer(void)
{
    static int firstTime = 1;
    if (firstTime) {
        firstTime = 0;
        iocshRegister(&startPVAServerFuncDef, startPVAServer);
    }
}

extern "C" {
    epicsExportRegistrar(registerStartPVAServer);
}
