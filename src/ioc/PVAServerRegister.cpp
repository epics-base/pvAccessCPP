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
namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

static pva::ServerContext::shared_pointer the_server;

static const iocshArg startPVAServerArg0 = { "providerNames", iocshArgString };
static const iocshArg *startPVAServerArgs[] = {
    &startPVAServerArg0};

static const iocshFuncDef startPVAServerFuncDef = {
    "startPVAServer", 1, startPVAServerArgs
};
static void startPVAServer(const iocshArgBuf *args)
{
    try {
        if(the_server) {
            std::cout<<"PVA server already running\n";
            return;
        }
        char *names = args[0].sval;
        if(!names) {
            the_server = pva::startPVAServer(pva::PVACCESS_ALL_PROVIDERS,0,true,true);
        } else {
            std::string providerNames(names);
            the_server = pva::startPVAServer(providerNames,0,true,true);
        }
    }catch(std::exception& e){
        std::cout<<"Error: "<<e.what()<<"\n";
    }
}

static const iocshArg *stopPVAServerArgs[] = {};
static const iocshFuncDef stopPVAServerFuncDef = {
    "stopPVAServer", 0, stopPVAServerArgs
};
static void stopPVAServer(const iocshArgBuf *args)
{
    try {
        if(!the_server) {
            std::cout<<"PVA server not running\n";
            return;
        }
        the_server.reset();
    }catch(std::exception& e){
        std::cout<<"Error: "<<e.what()<<"\n";
    }
}

static void registerStartPVAServer(void)
{
    iocshRegister(&startPVAServerFuncDef, startPVAServer);
    iocshRegister(&stopPVAServerFuncDef, stopPVAServer);
}

extern "C" {
    epicsExportRegistrar(registerStartPVAServer);
}
