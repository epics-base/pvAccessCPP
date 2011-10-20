/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/pvAccess.h>

namespace epics {
namespace pvAccess {

const char* Channel::ConnectionStateNames[] = { "NEVER_CONNECTED", "CONNECTED", "DISCONNECTED", "DESTROYED" };

}}
