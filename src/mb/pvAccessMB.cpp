/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifdef WITH_MICROBENCH
#include <pv/mb.h>
#endif

#define epicsExportSharedSymbols

#include "pv/pvAccessMB.h"

MB_DECLARE(channelGet, 100000);
