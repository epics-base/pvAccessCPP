/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <string>

#include <epicsSignal.h>

#include <pv/lock.h>

#define epicsExportSharedSymbols
#include <pv/logger.h>
#include <pv/clientFactory.h>
#include <pv/clientContextImpl.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

void ClientFactory::start()
{
    epicsSignalInstallSigAlarmIgnore();
    epicsSignalInstallSigPipeIgnore();

    getChannelProviderRegistry()->add("pva", createClientProvider, false);
}

void ClientFactory::stop()
{
    getChannelProviderRegistry()->remove("pva");
}
