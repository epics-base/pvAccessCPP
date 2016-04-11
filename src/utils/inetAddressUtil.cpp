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

#include <pv/pvType.h>
#include <pv/byteBuffer.h>
#include <pv/epicsException.h>

#define epicsExportSharedSymbols
#include <pv/inetAddressUtil.h>

using namespace std;
using namespace epics::pvData;

namespace epics {
namespace pvAccess {

void addDefaultBroadcastAddress(InetAddrVector* v, unsigned short p) {
    osiSockAddr pNewNode;
    pNewNode.ia.sin_family = AF_INET;
    // TODO this does not work in case of no active interfaces, should return 127.0.0.1 then
    pNewNode.ia.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    pNewNode.ia.sin_port = htons(p);
    v->push_back(pNewNode);
}

InetAddrVector* getBroadcastAddresses(SOCKET sock,
                                      unsigned short defaultPort) {
    ELLLIST as;
    ellInit(&as);
    osiSockAddr serverAddr;
    memset(&serverAddr, 0, sizeof(osiSockAddr));
    InetAddrVector * v = new InetAddrVector;
    osiSockDiscoverBroadcastAddresses(&as, sock, &serverAddr);
    for(ELLNODE * n = ellFirst(&as); n != NULL; n = ellNext(n))
    {
        osiSockAddrNode * sn = (osiSockAddrNode *)n;
        sn->addr.ia.sin_port = htons(defaultPort);
        // TODO discover possible duplicates
        v->push_back(sn->addr);
    }
    ellFree(&as);
    // add fallback address
    if (!v->size())
        addDefaultBroadcastAddress(v, defaultPort);
    return v;
}

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

    uint32_t ipv4Addr =
        ((uint32_t)(buffer->getByte()&0xFF))<<24 |
        ((uint32_t)(buffer->getByte()&0xFF))<<16 |
        ((uint32_t)(buffer->getByte()&0xFF))<<8  |
        ((uint32_t)(buffer->getByte()&0xFF));

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

osiSockAddr* intToIPv4Address(int32 addr) {
    osiSockAddr* ret = new osiSockAddr;
    ret->ia.sin_family = AF_INET;
    ret->ia.sin_addr.s_addr = htonl(addr);
    ret->ia.sin_port = 0;

    return ret;
}

int32 ipv4AddressToInt(const osiSockAddr& addr) {
    return (int32)ntohl(addr.ia.sin_addr.s_addr);
}

int32 parseInetAddress(const string & addr) {
    int32 retAddr;

    size_t dot = addr.find('.');
    if(dot==std::string::npos) THROW_BASE_EXCEPTION("Not an IPv4 address.");
    int byte = atoi(addr.substr(0, dot).c_str());
    if(byte<0||byte>255) THROW_BASE_EXCEPTION("Not an IPv4 address.");
    retAddr = byte;

    int num = dot+1;
    dot = addr.find('.', num);
    if(dot==std::string::npos) THROW_BASE_EXCEPTION("Not an IPv4 address.");
    byte = atoi(addr.substr(num, dot-num).c_str());
    if(byte<0||byte>255) THROW_BASE_EXCEPTION("Not an IPv4 address.");
    retAddr <<= 8;
    retAddr |= byte;

    num = dot+1;
    dot = addr.find('.', num);
    if(dot==std::string::npos) THROW_BASE_EXCEPTION("Not an IPv4 address.");
    byte = atoi(addr.substr(num, dot-num).c_str());
    if(byte<0||byte>255) THROW_BASE_EXCEPTION("Not an IPv4 address.");
    retAddr <<= 8;
    retAddr |= byte;

    num = dot+1;
    byte = atoi(addr.substr(num).c_str());
    if(byte<0||byte>255) THROW_BASE_EXCEPTION("Not an IPv4 address.");
    retAddr <<= 8;
    retAddr |= byte;

    return htonl(retAddr);
}

InetAddrVector* getSocketAddressList(const std::string & list, int defaultPort,
                                     const InetAddrVector* appendList) {
    InetAddrVector* iav = new InetAddrVector();

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
            iav->push_back(addr);
        subStart = list.find_first_not_of(" \t\r\n\v", subEnd);
    }

    if(subStart!=std::string::npos && subStart<len) {
        osiSockAddr addr;
        if (aToIPAddr(list.substr(subStart).c_str(), defaultPort, &addr.ia) == 0)
            iav->push_back(addr);
    }

    if(appendList!=NULL) {
        for(size_t i = 0; i<appendList->size(); i++)
            iav->push_back((*appendList)[i]);
    }
    return iav;
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

int getLoopbackNIF(osiSockAddr &loAddr, string const & localNIF, unsigned short port)
{
    if (!localNIF.empty())
    {
        if (aToIPAddr(localNIF.c_str(), port, &loAddr.ia) == 0)
            return 0;
        // else TODO log error
    }

    // fallback
    loAddr.ia.sin_family = AF_INET;
    loAddr.ia.sin_port = ntohs(port);
    loAddr.ia.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return 1;
}



#include <osiSock.h>
//#include <epicsAssert.h>
#include <errlog.h>

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
        /*ifDepenDebugPrintf(("discoverInterfaces(): status: 0x08x, ifconf.ifc_len: %d\n", status, ifconf.ifc_len));*/
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
        memmove(pIfreqList, pifreq, current_ifreqsize);

        /*ifDepenDebugPrintf (("discoverInterfaces(): found IFACE: %s len: 0x%x current_ifreqsize: 0x%x \n",
            pIfreqList->ifr_name,
            ifreq_size(pifreq),
            current_ifreqsize));*/

        /*
         * If its not an internet interface then dont use it
         */
        if ( pIfreqList->ifr_addr.sa_family != AF_INET ) {
            /*ifDepenDebugPrintf ( ("discoverInterfaces(): interface \"%s\" was not AF_INET\n", pIfreqList->ifr_name) );*/
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
                    /*ifDepenDebugPrintf ( ("discoverInterfaces(): net intf \"%s\" didnt match\n", pIfreqList->ifr_name) );*/
                    continue;
                }
                else
                    match = 1;
            }
        }

        status = socket_ioctl ( socket, SIOCGIFFLAGS, pIfreqList );
        if ( status ) {
            errlogPrintf ("discoverInterfaces(): net intf flags fetch for \"%s\" failed\n", pIfreqList->ifr_name);
            continue;
        }

        /*
         * dont bother with interfaces that have been disabled
         */
        if ( ! ( pIfreqList->ifr_flags & IFF_UP ) ) {
            /*ifDepenDebugPrintf ( ("discoverInterfaces(): net intf \"%s\" was down\n", pIfreqList->ifr_name) );*/
            continue;
        }

        /*
         * dont use the loop back interface, unless it maches pMatchAddr
         */
        if (!match) {
            if ( pIfreqList->ifr_flags & IFF_LOOPBACK ) {
                /*ifDepenDebugPrintf ( ("discoverInterfaces(): ignoring loopback interface: \"%s\"\n", pIfreqList->ifr_name) );*/
                continue;
            }
        }

        ifaceNode node;
        node.ifaceAddr.sa = pIfreqList->ifr_addr;

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
        if ( pIfreqList->ifr_flags & IFF_BROADCAST ) {
            status = socket_ioctl (socket, SIOCGIFBRDADDR, pIfreqList);
            if ( status ) {
                errlogPrintf ("discoverInterfaces(): net intf \"%s\": bcast addr fetch fail\n", pIfreqList->ifr_name);
                continue;
            }
            node.ifaceBCast.sa = pIfreqList->ifr_broadaddr;
            /*ifDepenDebugPrintf ( ( "found broadcast addr = %x\n", ntohl ( pNewNode->addr.ia.sin_addr.s_addr ) ) );*/
        }
#if defined (IFF_POINTOPOINT)
        else if ( pIfreqList->ifr_flags & IFF_POINTOPOINT ) {
            status = socket_ioctl ( socket, SIOCGIFDSTADDR, pIfreqList);
            if ( status ) {
                /*ifDepenDebugPrintf ( ("discoverInterfaces(): net intf \"%s\": pt to pt addr fetch fail\n", pIfreqList->ifr_name) );*/
                continue;
            }
            node.ifaceBCast.sa = pIfreqList->ifr_dstaddr;
        }
#endif
        else {
            // if it is a match, accept the interface even if it does not support broadcast (i.e. 127.0.0.1)
            if (match)
                node.ifaceBCast.sa.sa_family = AF_UNSPEC;
            else
            {
                /*ifDepenDebugPrintf ( ( "discoverInterfaces(): net intf \"%s\": not point to point or bcast?\n", pIfreqList->ifr_name ) );*/
                continue;
            }
        }

        /*ifDepenDebugPrintf ( ("discoverInterfaces(): net intf \"%s\" found\n", pIfreqList->ifr_name) );*/

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
        node.ifaceAddr.ia = pIfinfo->iiAddress.AddressIn;

        if (pIfinfo->iiFlags & IFF_BROADCAST) {
            const unsigned mask = pIfinfo->iiNetmask.AddressIn.sin_addr.s_addr;
            const unsigned bcast = pIfinfo->iiBroadcastAddress.AddressIn.sin_addr.s_addr;
            const unsigned addr = pIfinfo->iiAddress.AddressIn.sin_addr.s_addr;
            unsigned result = (addr & mask) | (bcast &~mask);
            node.ifaceBCast.ia.sin_family = AF_INET;
            node.ifaceBCast.ia.sin_addr.s_addr = result;
            node.ifaceBCast.ia.sin_port = htons ( 0 );
        }
        else {
            node.ifaceBCast.ia = pIfinfo->iiBroadcastAddress.AddressIn;
        }


        list.push_back(node);
    }

    free (pIfinfoList);
    return 0;
}

#endif

}
}
