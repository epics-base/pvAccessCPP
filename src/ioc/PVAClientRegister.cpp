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

#include <pv/pvAccess.h>
#include <pv/clientFactory.h>

#include <epicsExport.h>

using namespace epics::pvAccess;

static void stopPVAClient(void*)
{
    ClientFactory::stop();
}

static void registerStartPVAClient(void)
{
    ClientFactory::start();
    epicsAtExit(stopPVAClient, 0);
}


extern "C" {
    epicsExportRegistrar(registerStartPVAClient);
}
