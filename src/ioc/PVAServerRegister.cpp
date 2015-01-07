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

#include <epicsExport.h>

#include <pv/pvAccess.h>
#include <pv/serverContext.h>

using std::cout;
using std::endl;
using namespace epics::pvData;
using namespace epics::pvAccess;

class PVAServerCTX;
typedef std::tr1::shared_ptr<PVAServerCTX> PVAServerCTXPtr;

class PVAServerCTX :
    public std::tr1::enable_shared_from_this<PVAServerCTX>
{
public:
    POINTER_DEFINITIONS(PVAServerCTX);
    static PVAServerCTXPtr getPVAServerCTX();
    void start();
    void stop();
    virtual ~PVAServerCTX() {}
private:
    PVAServerCTX() {}
    shared_pointer getPtrSelf()
    {
        return shared_from_this();
    }
    ServerContext::shared_pointer ctx;
};

void PVAServerCTX::start()
{
   if(ctx.get()) {
        cout<< "PVAServer already started" << endl;
        return;
   }
   ctx = startPVAServer(PVACCESS_ALL_PROVIDERS,0,true,true);
}

void PVAServerCTX::stop()
{
   if(!ctx.get()) {
        cout<< "PVAServer already stopped" << endl;
        return;
   }
   ctx->destroy();
   ctx.reset();
   epicsThreadSleep(1.0);
}

PVAServerCTXPtr PVAServerCTX::getPVAServerCTX()
{
    static PVAServerCTXPtr pvPVAServerCTX;
    static Mutex mutex;
    Lock xx(mutex);

   if(!pvPVAServerCTX.get()) {
      pvPVAServerCTX = PVAServerCTXPtr(new PVAServerCTX());
   }
   return pvPVAServerCTX;
}


static const iocshFuncDef startPVAServerFuncDef = {
    "startPVAServer", 0, 0
};
extern "C" void startPVAServer(const iocshArgBuf *args)
{
    PVAServerCTX::getPVAServerCTX()->start();
}

static const iocshFuncDef stopPVAServerFuncDef = {
    "stopPVAServer", 0, 0
};
extern "C" void stopPVAServer(const iocshArgBuf *args)
{
    PVAServerCTX::getPVAServerCTX()->stop();
}


static void startPVAServerRegister(void)
{
    static int firstTime = 1;
    if (firstTime) {
        firstTime = 0;
        iocshRegister(&startPVAServerFuncDef, startPVAServer);
    }
}

static void stopPVAServerRegister(void)
{
    static int firstTime = 1;
    if (firstTime) {
        firstTime = 0;
        iocshRegister(&stopPVAServerFuncDef, stopPVAServer);
    }
}

extern "C" {
    epicsExportRegistrar(startPVAServerRegister);
    epicsExportRegistrar(stopPVAServerRegister);
}
