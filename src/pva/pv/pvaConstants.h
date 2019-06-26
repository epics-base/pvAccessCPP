/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVACONSTANTS_H_
#define PVACONSTANTS_H_

#include <compilerDependencies.h>

#ifdef epicsExportSharedSymbols
#   define pvaConstantsepicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/pvType.h>

#ifdef pvaConstantsepicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef pvaConstantsepicsExportSharedSymbols
#endif
#include <shareLib.h>

namespace epics {
namespace pvAccess {

/** PVA protocol magic number */
const epics::pvData::int8 PVA_MAGIC = static_cast<epics::pvData::int8>(0xCA);

const epics::pvData::int8 PVA_SERVER_PROTOCOL_REVISION = 2;
const epics::pvData::int8 PVA_CLIENT_PROTOCOL_REVISION = 2;

/** PVA protocol revision (implemented by this library). */
const epics::pvData::int8 PVA_PROTOCOL_REVISION EPICS_DEPRECATED = 1;

/** PVA version signature used to report this implementation version in header. */
const epics::pvData::int8 PVA_VERSION EPICS_DEPRECATED = 1;

/** Default PVA server port. */
const epics::pvData::int32 PVA_SERVER_PORT = 5075;

/** Default PVA beacon port. */
const epics::pvData::int32 PVA_BROADCAST_PORT = 5076;

/** PVA protocol message header size. */
const epics::pvData::int16 PVA_MESSAGE_HEADER_SIZE = 8;

/**
 * UDP maximum send message size.
 * MAX_UDP: 1500 (max of ethernet and 802.{2,3} MTU) - 20/40(IPv4/IPv6)
 * - 8(UDP) - some reserve (the MTU of Ethernet is currently independent
 * of its speed variant)
 */
const epics::pvData::int32 MAX_UDP_UNFRAGMENTED_SEND = 1440;

/**
 * UDP maximum receive message size.
 * MAX_UDP: 65535 (max UDP packet size) - 20/40(IPv4/IPv6) - 8(UDP)
 */
const epics::pvData::int32 MAX_UDP_RECV = 65487;

/** TCP maximum receive message size. */
const epics::pvData::int32 MAX_TCP_RECV = 1024*16;

/** Maximum number of search requests in one search message. */
const epics::pvData::int32 MAX_SEARCH_BATCH_COUNT = 0x7FFF;  // 32767

/** Default priority (corresponds to POSIX SCHED_OTHER) */
const epics::pvData::int16 PVA_DEFAULT_PRIORITY = 0;

/** Unreasonable channel name length. */
const epics::pvData::uint32 MAX_CHANNEL_NAME_LENGTH = 500;

/** Invalid data type. */
const epics::pvData::int16 INVALID_DATA_TYPE = 0xFFFF;

/** Invalid IOID. */
const epics::pvData::int32 INVALID_IOID = 0;

/** Default PVA provider name. */
epicsShareExtern const std::string PVACCESS_DEFAULT_PROVIDER;

/** "All-providers registered" PVA provider name. */
epicsShareExtern const std::string PVACCESS_ALL_PROVIDERS;

/** Name of the system env. variable to turn on debugging. */
epicsShareExtern const std::string PVACCESS_DEBUG;
}
}

#endif /* PVACONSTANTS_H_ */
