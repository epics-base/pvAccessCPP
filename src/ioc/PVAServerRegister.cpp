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
#include <initHooks.h>
#include <epicsExit.h>

#include <pv/pvAccess.h>
#include <pv/serverContext.h>
#include <pv/iocshelper.h>

#include <epicsExport.h>

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

namespace {

pvd::Mutex the_server_lock;
pva::ServerContext::shared_pointer the_server;

void startitup() {
    the_server = pva::ServerContext::create(pva::ServerContext::Config()
                                            .config(pva::ConfigurationBuilder()
                                                    // default to all providers instead of just "local"
                                                    .add("EPICS_PVAS_PROVIDER_NAMES", pva::PVACCESS_ALL_PROVIDERS)
                                                    .push_map()
                                                    // prefer to use EPICS_PVAS_PROVIDER_NAMES
                                                    // from environment
                                                    .push_env()
                                                    .build()));
}

void startPVAServer(const char *names)
{
    try {
        if(names && names[0]!='\0') {
            printf("Warning: startPVAServer() no longer accepts provider list as argument.\n"
                       "         Instead place the following before calling startPVAServer() and iocInit()\n"
                       "  epicsEnvSet(\"EPICS_PVAS_PROVIDER_NAMES\", \"%s\")\n",
                    names);
        }
        pvd::Lock G(the_server_lock);
        if(the_server) {
            std::cout<<"PVA server already running\n";
            return;
        }
        startitup();
    }catch(std::exception& e){
        std::cout<<"Error: "<<e.what()<<"\n";
    }
}

void stopPVAServer()
{
    try {
        pvd::Lock G(the_server_lock);
        if(!the_server) {
            std::cout<<"PVA server not running\n";
            return;
        }
        the_server.reset();
    }catch(std::exception& e){
        std::cout<<"Error: "<<e.what()<<"\n";
    }
}

void pvasr(int lvl)
{
    try {
        pva::ServerContext::shared_pointer serv;
        {
            pvd::Lock G(the_server_lock);
            serv = the_server;
        }
        if(!serv) {
            std::cout<<"PVA server not running\n";
        } else {
            serv->printInfo(lvl);
        }
        std::cout.flush();
    }catch(std::exception& e){
        std::cout<<"Error: "<<e.what()<<std::endl;
    }
}

void pva_server_cleanup(void *)
{
    stopPVAServer();
}

void initStartPVAServer(initHookState state)
{
    pvd::Lock G(the_server_lock);
    if(state==initHookAfterIocRunning && !the_server) {
        epicsAtExit(&pva_server_cleanup, 0);
        startitup();

    } else if(state==initHookAtIocPause) {
        the_server.reset();
    }
}

void registerStartPVAServer(void)
{
    epics::iocshRegister<const char*, &startPVAServer>("startPVAServer", "provider names");
    epics::iocshRegister<&stopPVAServer>("stopPVAServer");
    epics::iocshRegister<int, &pvasr>("pvasr", "detail");
    initHookRegister(&initStartPVAServer);
}

} // namespace

extern "C" {
    epicsExportRegistrar(registerStartPVAServer);
}
