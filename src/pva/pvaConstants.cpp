/**
* Copyright - See the COPYRIGHT that is included with this distribution.
* pvAccessCPP is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
*/

#define epicsExportSharedSymbols
#include <pv/pvaConstants.h>

namespace epics { namespace pvAccess {

const std::string PVACCESS_DEFAULT_PROVIDER("local");
const std::string PVACCESS_ALL_PROVIDERS("<all>");
const std::string PVACCESS_DEBUG("EPICS_PVA_DEBUG");
const std::string PVA_TCP_PROTOCOL("tcp");
const std::string PVA_UDP_PROTOCOL("udp");

}}
