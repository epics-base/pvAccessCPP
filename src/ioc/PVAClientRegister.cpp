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

#include <epicsExport.h>

static void registerStartPVAClient(void) {}

extern "C" {
    epicsExportRegistrar(registerStartPVAClient);
}
