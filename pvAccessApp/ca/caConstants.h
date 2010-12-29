/*
 * constants.hpp
 *
 *  Created on: Oct 8, 2010
 *      Author: Miha Vitorovic
 */

#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#include <pvType.h>

using namespace epics::pvData;

namespace epics {
    namespace pvAccess {

        /** CA protocol magic number */
        const int8 CA_MAGIC = 0xCA;

        /** CA protocol major revision (implemented by this library). */
        const int8 CA_MAJOR_PROTOCOL_REVISION = 5;

        /** CA protocol minor revision (implemented by this library). */
        const int8 CA_MINOR_PROTOCOL_REVISION = 0;

        /** Unknown CA protocol minor revision. */
        const int8 CA_UNKNOWN_MINOR_PROTOCOL_REVISION = 0;

        /** CA magic/major version signature (e.g. 0xCA50). */
        const int16 CA_MAGIC_AND_MAJOR_VERSION = CA_MAGIC<<8
                |CA_MAJOR_PROTOCOL_REVISION<<4;

        /** CA magic/version signature (e.g. 0xCA51). */
        const int16 CA_MAGIC_AND_VERSION = CA_MAGIC<<8
                |CA_MAJOR_PROTOCOL_REVISION<<4|CA_MINOR_PROTOCOL_REVISION;

        /** CA protocol port base. */
        const int32 CA_PORT_BASE = 5056;

        /** Default CA server port. */
        const int32 CA_SERVER_PORT = CA_PORT_BASE+2*CA_MAJOR_PROTOCOL_REVISION;

        /** Default CA beacon port. */
        const int32 CA_BROADCAST_PORT = CA_SERVER_PORT+1;

        /** CA protocol message header size. */
        const int16 CA_MESSAGE_HEADER_SIZE = 8;

        /**
         * UDP maximum send message size.
         * MAX_UDP: 1500 (max of ethernet and 802.{2,3} MTU) - 20/40(IPv4/IPv6)
         * - 8(UDP) - some reserve (the MTU of Ethernet is currently independent
         * of its speed variant)
         */
        const int32 MAX_UDP_SEND = 1440;

        /** UDP maximum receive message size. */
        const int32 MAX_UDP_RECV = 0xFFFF+16;

        /** TCP maximum receive message size. */
        const int32 MAX_TCP_RECV = 1024*16;

        /** Maximum number of search requests in one search message. */
        const int32 MAX_SEARCH_BATCH_COUNT = 0xFFFF;

        /** Default priority (corresponds to POSIX SCHED_OTHER) */
        const int16 CA_DEFAULT_PRIORITY = 0;

        /** Unreasonable channel name length. */
        const int32 UNREASONABLE_CHANNEL_NAME_LENGTH = 500;

        /** Invalid data type. */
        const int16 INVALID_DATA_TYPE = (int16)0xFFFF;

    }
}

#endif /* CONSTANTS_H_ */
