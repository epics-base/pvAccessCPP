/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef INETADDRESSUTIL_H_
#define INETADDRESSUTIL_H_

#include <vector>

#include <osiSock.h>
#include <shareLib.h>

#include <pv/pvType.h>
#include <pv/byteBuffer.h>

namespace epics {
namespace pvAccess {

typedef std::vector<osiSockAddr> InetAddrVector;

struct ifaceNode {
    osiSockAddr addr, //!< Our address
                peer, //!< point to point peer
                bcast,//!< sub-net broadcast address
                mask; //!< Net mask
    bool loopback,
         validP2P, //!< true if peer has been set.
         validBcast; //!< true if bcast and mask have been set
    ifaceNode();
};
typedef std::vector<ifaceNode> IfaceNodeVector;
epicsShareFunc int discoverInterfaces(IfaceNodeVector &list, SOCKET socket, const osiSockAddr *pMatchAddr = 0);

/**
 * Encode IPv4 address as IPv6 address.
 * @param buffer byte-buffer where to put encoded data.
 * @param address address to encode.
 */
epicsShareFunc void encodeAsIPv6Address(epics::pvData::ByteBuffer* buffer, const osiSockAddr* address);

/**
 * Decode IPv6 address (as IPv4 address).
 * @param buffer byte-buffer where to get encoded data.
 * @param address address where to decode.
 * @return success status (true on success).
 */
epicsShareFunc bool decodeAsIPv6Address(epics::pvData::ByteBuffer* buffer, osiSockAddr* address);

/**
 * Check if an IPv4 address is a multicast address.
 * @param address IPv4 address to check.
 * @return true if the adress is a multicast address.
 */
epicsShareFunc bool isMulticastAddress(const osiSockAddr* address);

/**
 * Parse space delimited addresss[:port] string and populate array of <code>InetSocketAddress</code>.
 * @param ret results stored hre
 * @param list  space delimited addresss[:port] string.
 * @param defaultPort   port take if not specified.
 * @param appendList    list to be appended.
 * @return  array of <code>InetSocketAddress</code>.
 */
epicsShareFunc void getSocketAddressList(InetAddrVector& ret, const std::string & list, int defaultPort,
                                         const InetAddrVector* appendList = NULL);

epicsShareFunc std::string inetAddressToString(const osiSockAddr &addr,
        bool displayPort = true, bool displayHex = false);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// comparators for osiSockAddr

struct comp_osiSock_lt {
    bool operator()(const osiSockAddr& a, const osiSockAddr& b) const {
        if(a.sa.sa_family<b.sa.sa_family) return true;
        if((a.sa.sa_family==b.sa.sa_family)&&(a.ia.sin_addr.s_addr
                                              <b.ia.sin_addr.s_addr)) return true;
        if((a.sa.sa_family==b.sa.sa_family)&&(a.ia.sin_addr.s_addr
                                              ==b.ia.sin_addr.s_addr)&&(a.ia.sin_port
                                                      <b.ia.sin_port)) return true;
        return false;
    }
};

}} // namespace epics::pvAccess

#endif /* INETADDRESSUTIL_H_ */
