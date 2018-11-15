/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <vector>
#include <cstring>
#include <cstdlib>
#include <sstream>

#include <osiSock.h>
#include <ellLib.h>
#include <errlog.h>

#include <pv/pvType.h>
#include <pv/byteBuffer.h>
#include <pv/epicsException.h>

#define epicsExportSharedSymbols
#include <pv/inetAddressUtil.h>

// RTEMS 4.9 doesn't define this, but does implement SIOCGIFNETMASK
// and stores under the ifr_addr union member.
#ifndef ifr_netmask
#  define ifr_netmask ifr_addr
#endif

using namespace std;
using namespace epics::pvData;

namespace epics {
namespace pvAccess {

void encodeAsIPv6Address(ByteBuffer* buffer, const osiSockAddr* address) {
    // IPv4 compatible IPv6 address
    // first 80-bit are 0
    buffer->putLong(0);
    buffer->putShort(0);
    // next 16-bits are 1
    buffer->putShort(0xFFFF);
    // following IPv4 address in big-endian (network) byte order
    uint32_t ipv4Addr = ntohl(address->ia.sin_addr.s_addr);
    buffer->putByte((int8)((ipv4Addr>>24)&0xFF));
    buffer->putByte((int8)((ipv4Addr>>16)&0xFF));
    buffer->putByte((int8)((ipv4Addr>>8)&0xFF));
    buffer->putByte((int8)(ipv4Addr&0xFF));
}

bool decodeAsIPv6Address(ByteBuffer* buffer, osiSockAddr* address) {

    // IPv4 compatible IPv6 address expected
    // first 80-bit are 0
    if (buffer->getLong() != 0) return false;
    if (buffer->getShort() != 0) return false;
    int16 ffff = buffer->getShort();
    // allow all zeros address
    //if (ffff != (int16)0xFFFF) return false;

    uint32 ipv4Addr = uint8(buffer->getByte());
    ipv4Addr <<= 8;
    ipv4Addr |= uint8(buffer->getByte());
    ipv4Addr <<= 8;
    ipv4Addr |= uint8(buffer->getByte());
    ipv4Addr <<= 8;
    ipv4Addr |= uint8(buffer->getByte());

    if (ffff != (int16)0xFFFF && ipv4Addr != (uint32_t)0)
        return false;

    address->ia.sin_addr.s_addr = htonl(ipv4Addr);

    return true;
}

bool isMulticastAddress(const osiSockAddr* address) {
    uint32_t ipv4Addr = ntohl(address->ia.sin_addr.s_addr);
    uint8_t msB = (uint8_t)((ipv4Addr>>24)&0xFF);
    return msB >= 224 && msB <= 239;
}

void getSocketAddressList(InetAddrVector& ret,
                          const std::string & list, int defaultPort,
                                     const InetAddrVector* appendList) {
    ret.clear();

    // skip leading spaces
    size_t len = list.length();
    size_t subStart = 0;
    while (subStart < len && isspace(list[subStart]))
        subStart++;

    // parse string
    size_t subEnd;
    while((subEnd = list.find(' ', subStart))!=std::string::npos) {
        string address = list.substr(subStart, (subEnd-subStart));
        osiSockAddr addr;
        if (aToIPAddr(address.c_str(), defaultPort, &addr.ia) == 0)
            ret.push_back(addr);
        subStart = list.find_first_not_of(" \t\r\n\v", subEnd);
    }

    if(subStart!=std::string::npos && subStart<len) {
        osiSockAddr addr;
        if (aToIPAddr(list.substr(subStart).c_str(), defaultPort, &addr.ia) == 0)
            ret.push_back(addr);
    }

    if(appendList!=NULL) {
        for(size_t i = 0; i<appendList->size(); i++)
            ret.push_back((*appendList)[i]);
    }
}

string inetAddressToString(const osiSockAddr &addr,
                           bool displayPort, bool displayHex) {
    stringstream saddr;

    int ipa = ntohl(addr.ia.sin_addr.s_addr);

    saddr<<((int)(ipa>>24)&0xFF)<<'.';
    saddr<<((int)(ipa>>16)&0xFF)<<'.';
    saddr<<((int)(ipa>>8)&0xFF)<<'.';
    saddr<<((int)ipa&0xFF);
    if(displayPort) saddr<<":"<<ntohs(addr.ia.sin_port);
    if(displayHex) saddr<<" ("<<hex<<ntohl(addr.ia.sin_addr.s_addr)
                            <<")";

    return saddr.str();
}

ifaceNode::ifaceNode()
{
    memset(&addr, 0, sizeof(addr));
    memset(&peer, 0, sizeof(peer));
    memset(&bcast, 0, sizeof(bcast));
    memset(&mask, 0, sizeof(mask));
    validBcast = validP2P = loopback = false;
}

static
void checkNode(ifaceNode& node)
{
    if(node.validBcast) {
        /* Cross-check between addr, mask, and bcast to detect incorrect broadcast
         * address.  Why do admins insist on setting this seperately?!?
         */
        uint32 addr = ntohl(node.addr.ia.sin_addr.s_addr),
               mask = ntohl(node.mask.ia.sin_addr.s_addr),
               bcast= ntohl(node.bcast.ia.sin_addr.s_addr),
               bcast_expect = (addr & mask) | ~mask;

        if(bcast == ntohl(INADDR_BROADCAST)) {
            // translate global broadcast to iface broadcast.
            // Windows (at least) will give us this sometimes.
            bcast = bcast_expect;
            node.bcast.ia.sin_addr.s_addr = htonl(bcast);
        }

        if(bcast != bcast_expect) {
            errlogPrintf("Warning: Inconsistent broadcast address on interface %08x/%08x.  expect %08x found %08x.\n",
                         (unsigned)addr, (unsigned)mask, (unsigned)bcast_expect, (unsigned)bcast);
        }
    }
}

#if !defined(_WIN32)

/*
 * Determine the size of an ifreq structure
 * Made difficult by the fact that addresses larger than the structure
 * size may be returned from the kernel.
 */
static size_t ifreqSize ( struct ifreq *pifreq )
{
    size_t        size;

    size = ifreq_size ( pifreq );
    if ( size < sizeof ( *pifreq ) ) {
        size = sizeof ( *pifreq );
    }
    return size;
}

/*
 * Move to the next ifreq structure
 */
static struct ifreq * ifreqNext ( struct ifreq *pifreq )
{
    struct ifreq *ifr;

    ifr = ( struct ifreq * )( ifreqSize (pifreq) + ( char * ) pifreq );
    return ifr;
}

int discoverInterfaces(IfaceNodeVector &list, SOCKET socket, const osiSockAddr *pMatchAddr)
{
    static const unsigned           nelem = 100;
    int                             status;
    struct ifconf                   ifconf;
    struct ifreq                    *pIfreqList;
    struct ifreq                    *pIfreqListEnd;
    struct ifreq                    *pifreq;
    struct ifreq                    *pnextifreq;
    int                             match;

    /*
     * use pool so that we avoid using too much stack space
     *
     * nelem is set to the maximum interfaces
     * on one machine here
     */
    pIfreqList = (struct ifreq *) calloc ( nelem, sizeof(*pifreq) );
    if (!pIfreqList) {
        errlogPrintf ("discoverInterfaces(): no memory to complete request\n");
        return -1;
    }

    ifconf.ifc_len = nelem * sizeof(*pifreq);
    ifconf.ifc_req = pIfreqList;
    status = socket_ioctl (socket, SIOCGIFCONF, &ifconf);
    if (status < 0 || ifconf.ifc_len == 0) {
        errlogPrintf ("discoverInterfaces(): unable to fetch network interface configuration\n");
        free (pIfreqList);
        return -1;
    }

    pIfreqListEnd = (struct ifreq *) (ifconf.ifc_len + (char *) pIfreqList);
    pIfreqListEnd--;

    for ( pifreq = pIfreqList; pifreq <= pIfreqListEnd; pifreq = pnextifreq ) {
        uint32_t  current_ifreqsize;

        /*
         * find the next ifreq
         */
        pnextifreq = ifreqNext (pifreq);

        /* determine ifreq size */
        current_ifreqsize = ifreqSize ( pifreq );
        /* copy current ifreq to aligned bufferspace (to start of pIfreqList buffer) */
        /* be careful as we re-use part of this space several times below.
         * Any member other than ifr_name is invalidated by an ioctl() call
         */
        memmove(pIfreqList, pifreq, current_ifreqsize);

        /*
         * If its not an internet interface then dont use it
         */
        if ( pIfreqList->ifr_addr.sa_family != AF_INET ) {
            continue;
        }

        /*
         * if it isnt a wildcarded interface then look for
         * an exact match
         */
        match = 0;
        if ( pMatchAddr && pMatchAddr->sa.sa_family != AF_UNSPEC ) {
            if ( pMatchAddr->sa.sa_family != AF_INET ) {
                continue;
            }
            if ( pMatchAddr->ia.sin_addr.s_addr != htonl (INADDR_ANY) ) {
                struct sockaddr_in *pInetAddr = (struct sockaddr_in *) &pIfreqList->ifr_addr;
                if ( pInetAddr->sin_addr.s_addr != pMatchAddr->ia.sin_addr.s_addr ) {
                    continue;
                }
                else
                    match = 1;
            }
        }

        ifaceNode node;
        node.addr.sa = pIfreqList->ifr_addr;

        status = socket_ioctl ( socket, SIOCGIFFLAGS, pIfreqList );
        if ( status ) {
            errlogPrintf ("discoverInterfaces(): net intf flags fetch for \"%s\" failed\n", pIfreqList->ifr_name);
            continue;
        }

        unsigned short ifflags = pIfreqList->ifr_flags;
        node.loopback = ifflags & IFF_LOOPBACK;

        /*
         * dont bother with interfaces that have been disabled
         */
        if ( ! ( ifflags & IFF_UP ) ) {
            continue;
        }

        /*
         * dont use the loop back interface, unless it maches pMatchAddr
         */
        if (!match) {
            if ( ifflags & IFF_LOOPBACK ) {
                continue;
            }
        }

        /*
         * If this is an interface that supports
         * broadcast fetch the broadcast address.
         *
         * Otherwise if this is a point to point
         * interface then use the destination address.
         *
         * Otherwise CA will not query through the
         * interface.
         */
        if ( ifflags & IFF_BROADCAST ) {
            status = socket_ioctl (socket, SIOCGIFBRDADDR, pIfreqList);
            if ( status ) {
                errlogPrintf ("discoverInterfaces(): net intf \"%s\": bcast addr fetch fail\n", pIfreqList->ifr_name);
                continue;
            }
            node.bcast.sa = pIfreqList->ifr_broadaddr;

            status = socket_ioctl (socket, SIOCGIFNETMASK, pIfreqList);
            if ( status ) {
                errlogPrintf ("discoverInterfaces(): net intf \"%s\": netmask fetch fail\n", pIfreqList->ifr_name);
                continue;
            }
            node.mask.sa = pIfreqList->ifr_netmask;

            checkNode(node);

            node.validBcast = true;
        }
#if defined (IFF_POINTOPOINT)
        else if ( ifflags & IFF_POINTOPOINT ) {
            status = socket_ioctl ( socket, SIOCGIFDSTADDR, pIfreqList);
            if ( status ) {
                continue;
            }
            node.peer.sa = pIfreqList->ifr_dstaddr;
            node.validP2P = true;
        }
#endif
        else {
            // if it is a match, accept the interface even if it does not support broadcast (i.e. 127.0.0.1 or point to point)
            if (!match)
            {
                continue;
            }
        }

        list.push_back(node);
    }

    free ( pIfreqList );
    return 0;
}


#else

#define VC_EXTRALEAN
#include <winsock2.h>
#include <ws2tcpip.h>

int discoverInterfaces(IfaceNodeVector &list, SOCKET socket, const osiSockAddr *pMatchAddr)
{
    int             	status;
    INTERFACE_INFO      *pIfinfo;
    INTERFACE_INFO      *pIfinfoList;
    unsigned			nelem;
    int					numifs;
    DWORD				cbBytesReturned;
    int					match;

    /* only valid for winsock 2 and above
    TODO resolve dllimport compilation problem and uncomment this check
    if (wsaMajorVersion() < 2 ) {
        fprintf(stderr, "Need to set EPICS_CA_AUTO_ADDR_LIST=NO for winsock 1\n");
        return -1;
    }
    */

    nelem = 100;
    pIfinfoList = (INTERFACE_INFO *) calloc(nelem, sizeof(INTERFACE_INFO));
    if(!pIfinfoList) {
        return -1;
    }

    status = WSAIoctl (socket, SIO_GET_INTERFACE_LIST,
                       NULL, 0,
                       (LPVOID)pIfinfoList, nelem*sizeof(INTERFACE_INFO),
                       &cbBytesReturned, NULL, NULL);

    if (status != 0 || cbBytesReturned == 0) {
        fprintf(stderr, "WSAIoctl SIO_GET_INTERFACE_LIST failed %d\n",WSAGetLastError());
        free(pIfinfoList);
        return -1;
    }

    numifs = cbBytesReturned/sizeof(INTERFACE_INFO);
    for (pIfinfo = pIfinfoList; pIfinfo < (pIfinfoList+numifs); pIfinfo++) {

        /*
         * dont bother with interfaces that have been disabled
         */
        if (!(pIfinfo->iiFlags & IFF_UP)) {
            continue;
        }

        /*
         * If its not an internet interface then dont use it
         * + work around WS2 bug
         */
        if (pIfinfo->iiAddress.Address.sa_family != AF_INET) {
            if (pIfinfo->iiAddress.Address.sa_family == 0) {
                pIfinfo->iiAddress.Address.sa_family = AF_INET;
            }
            else
                continue;
        }

        /*
         * if it isnt a wildcarded interface then look for
         * an exact match
         */
        match = 0;
        if (pMatchAddr && pMatchAddr->sa.sa_family != AF_UNSPEC) {
            if (pIfinfo->iiAddress.Address.sa_family != pMatchAddr->sa.sa_family) {
                continue;
            }
            if (pIfinfo->iiAddress.Address.sa_family != AF_INET) {
                continue;
            }
            if (pMatchAddr->sa.sa_family != AF_INET) {
                continue;
            }
            if (pMatchAddr->ia.sin_addr.s_addr != htonl(INADDR_ANY)) {
                if (pIfinfo->iiAddress.AddressIn.sin_addr.s_addr != pMatchAddr->ia.sin_addr.s_addr) {
                    continue;
                }
                else
                    match = 1;
            }
        }

        /*
         * dont use the loop back interface, unless it maches pMatchAddr
         */
        if (!match) {
            if (pIfinfo->iiFlags & IFF_LOOPBACK) {
                continue;
            }
        }

        ifaceNode node;
        node.loopback = pIfinfo->iiFlags & IFF_LOOPBACK;
        node.addr.ia = pIfinfo->iiAddress.AddressIn;

        if (pIfinfo->iiFlags & IFF_BROADCAST) {
            node.mask.ia = pIfinfo->iiNetmask.AddressIn;
            node.bcast.ia = pIfinfo->iiBroadcastAddress.AddressIn;
            node.validBcast = true;
        }
        else if (pIfinfo->iiFlags & IFF_POINTTOPOINT) {
            node.peer.ia = pIfinfo->iiNetmask.AddressIn;
            node.validP2P = true;
        }

        checkNode(node);

        list.push_back(node);
    }

    free (pIfinfoList);
    return 0;
}

#endif

}
}
