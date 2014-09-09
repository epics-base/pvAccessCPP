/**
* Copyright - See the COPYRIGHT that is included with this distribution.
* pvAccessCPP is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
*/

#define epicsExportSharedSymbols
#include <pv/security.h>

using namespace epics::pvAccess;

NoSecurityPlugin::shared_pointer NoSecurityPlugin::INSTANCE(new NoSecurityPlugin());

