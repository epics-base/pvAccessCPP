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


// TODO implement using smart pointers

namespace epics {
namespace pvAccess {

typedef std::vector<osiSockAddr> InetAddrVector;

/**
 * Returns a vector containing all the IPv4 broadcast addresses on this machine.
 * IPv6 doesn't have a local broadcast address.
 * Conversion of the defaultPort to network byte order performed by
 * the function.
 */
epicsShareFunc InetAddrVector* getBroadcastAddresses(SOCKET sock, unsigned short defaultPort);

struct ifaceNode {
    osiSockAddr ifaceAddr, ifaceBCast;
};
typedef std::vector<ifaceNode> IfaceNodeVector;
epicsShareFunc int discoverInterfaces(IfaceNodeVector &list, SOCKET socket, const osiSockAddr *pMatchAddr = 0);

/**
 * Returns NIF index for given interface address, or -1 on failure.
 */
epicsShareFunc int discoverInterfaceIndex(SOCKET socket, const osiSockAddr *pMatchAddr);

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
 * Convert an integer into an IPv4 INET address.
 * @param addr integer representation of a given address.
 * @return IPv4 INET address.
 */
epicsShareFunc osiSockAddr* intToIPv4Address(epics::pvData::int32 addr);

/**
 * Convert an IPv4 INET address to an integer.
 * @param addr  IPv4 INET address.
 * @return integer representation of a given address.
 */
epicsShareFunc epics::pvData::int32 ipv4AddressToInt(const osiSockAddr& addr);

/**
 * Parse space delimited addresss[:port] string and return array of <code>InetSocketAddress</code>.
 * @param list  space delimited addresss[:port] string.
 * @param defaultPort   port take if not specified.
 * @param appendList    list to be appended.
 * @return  array of <code>InetSocketAddress</code>.
 */
epicsShareFunc InetAddrVector* getSocketAddressList(const std::string & list, int defaultPort,
        const InetAddrVector* appendList = NULL);

epicsShareFunc std::string inetAddressToString(const osiSockAddr &addr,
        bool displayPort = true, bool displayHex = false);

epicsShareFunc int getLoopbackNIF(osiSockAddr& loAddr, std::string const & localNIF, unsigned short port);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// comparators for osiSockAddr

struct comp_osiSockAddrPtr {
    bool operator()(osiSockAddr const *a, osiSockAddr const *b) const {
        if(a->sa.sa_family<b->sa.sa_family) return true;
        if((a->sa.sa_family==b->sa.sa_family)&&(a->ia.sin_addr.s_addr
                                                <b->ia.sin_addr.s_addr)) return true;
        if((a->sa.sa_family==b->sa.sa_family)&&(a->ia.sin_addr.s_addr
                                                ==b->ia.sin_addr.s_addr)&&(a->ia.sin_port
                                                        <b->ia.sin_port)) return true;
        return false;
    }
};

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

//TODO if unordered map is used instead of map we can use sockAddrAreIdentical routine from osiSock.h
struct comp_osiSockAddr {
    bool operator()(osiSockAddr const a, osiSockAddr const b) const {
        if(a.sa.sa_family<b.sa.sa_family) return true;
        if((a.sa.sa_family==b.sa.sa_family)&&(a.ia.sin_addr.s_addr
                                              <b.ia.sin_addr.s_addr)) return true;
        if((a.sa.sa_family==b.sa.sa_family)&&(a.ia.sin_addr.s_addr
                                              ==b.ia.sin_addr.s_addr)&&(a.ia.sin_port
                                                      <b.ia.sin_port)) return true;
        return false;
    }
};

}
}

#endif /* INETADDRESSUTIL_H_ */
