/*PVAClientRegister.cpp */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2013.08.05
 */

/* Author: Marty Kraimer */

#include <cstddef>
#include <cstdlib>
#include <cstddef>
#include <string>
#include <cstdio>
#include <memory>
#include <iostream>

#include <iocsh.h>
#include <epicsExit.h>

#include <epicsExport.h>

#include <pv/pvAccess.h>
#include <pv/clientFactory.h>

using std::cout;
using std::endl;
using namespace epics::pvData;
using namespace epics::pvAccess;


static const iocshFuncDef startPVAClientFuncDef = {
    "startPVAClient", 0, 0
};

extern "C" void startPVAClient(const iocshArgBuf *args)
{
    ClientFactory::start();
}


static void registerStartPVAClient(void)
{
    static int firstTime = 1;
    if (firstTime) {
        firstTime = 0;
        iocshRegister(&startPVAClientFuncDef, startPVAClient);
    }
}


extern "C" {
    epicsExportRegistrar(registerStartPVAClient);
}
