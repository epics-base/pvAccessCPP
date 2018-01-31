/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <string>

#include <pv/lock.h>

#define epicsExportSharedSymbols
#include <pv/clientFactory.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

void ClientFactory::start()
{
    // 'pva' registration in ChannelAccessFactory.cpp
}

void ClientFactory::stop() {}
