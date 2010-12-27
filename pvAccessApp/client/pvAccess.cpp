/*pvAccess.cpp*/
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvDataCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
#include <pvAccess.h>

namespace epics { namespace pvAccess {

const char* ConnectionStateNames[] = { "NEVER_CONNECTED", "CONNECTED", "DISCONNECTED", "DESTROYED" };

}}