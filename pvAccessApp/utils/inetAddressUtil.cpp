/*
 * inetAddressUtil.cpp
 *
 *  Created on: Nov 12, 2010
 *      Author: Miha Vitorovic
 */
/* pvAccess */
#include "inetAddressUtil.h"

/* pvData */
#include <byteBuffer.h>

/* EPICSv3 */
#include <osiSock.h>
#include <ellLib.h>
#include <epicsAssert.h>
#include <epicsException.h>
#include <errlog.h>

/* standard */
#include <vector>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

/*
 * In newer BSD systems, the socket address is variable-length, and
 * there's an "sa_len" field giving the length of the structure;
 * this allows socket addresses to be longer than 2 bytes of family
 * and 14 bytes of data.
 *
 * Some commercial UNIXes use the old BSD scheme, some use the RFC 2553
 * variant of the old BSD scheme (with "struct sockaddr_storage" rather
 * than "struct sockaddr"), and some use the new BSD scheme.
 *
 * Some versions of GNU libc use neither scheme, but has an "SA_LEN()"
 * macro that determines the size based on the address family.  Other
 * versions don't have "SA_LEN()" (as it was in drafts of RFC 2553
 * but not in the final version).
 *
 * We assume that a UNIX that doesn't have "getifaddrs()" and doesn't have
 * SIOCGLIFCONF, but has SIOCGIFCONF, uses "struct sockaddr" for the
 * address in an entry returned by SIOCGIFCONF.
 */
#ifndef SA_LEN
#ifdef HAVE_SOCKADDR_SA_LEN
#define SA_LEN(addr)    ((addr)->sa_len)
#else /* HAVE_SOCKADDR_SA_LEN */
#define SA_LEN(addr)    (sizeof (struct sockaddr))
#endif /* HAVE_SOCKADDR_SA_LEN */
#endif /* SA_LEN */


using namespace std;
using namespace epics::pvData;

namespace epics {
    namespace pvAccess {

        void addDefaultBroadcastAddress(InetAddrVector* v, in_port_t p) {
            osiSockAddr* pNewNode = new osiSockAddr;
            pNewNode->ia.sin_family = AF_INET;
            pNewNode->ia.sin_addr.s_addr = htonl(INADDR_BROADCAST);
            pNewNode->ia.sin_port = htons(p);
            v->push_back(pNewNode);
        }

        /* port of osiSockDiscoverBroadcastAddresses() in
         * epics/base/src/libCom/osi/os/default/osdNetIntf.c
         */
        InetAddrVector* getBroadcastAddresses(SOCKET sock,
                in_port_t defaultPort) {
            static const unsigned nelem = 100;
            int status;
            struct ifconf ifconf;
            struct ifreq* pIfreqList;
            struct ifreq* pifreq;
            struct ifreq ifrBuff;
            osiSockAddr* pNewNode;

            InetAddrVector* retVector = new InetAddrVector();

            /*
             * use pool so that we avoid using too much stack space
             *
             * nelem is set to the maximum interfaces
             * on one machine here
             */
            pIfreqList = new ifreq[nelem];
            if(!pIfreqList) {
                errlogSevPrintf(errlogMajor,
                        "getBroadcastAddresses(): no memory to complete request");
                addDefaultBroadcastAddress(retVector, defaultPort);
                return retVector;
            }

            // get number of interfaces
            ifconf.ifc_len = nelem*sizeof(ifreq);
            ifconf.ifc_req = pIfreqList;
            memset(ifconf.ifc_req, 0, ifconf.ifc_len);
            status = ioctl(sock, SIOCGIFCONF, &ifconf);
            if(status<0||ifconf.ifc_len==0) {
                errlogSevPrintf(errlogMinor,
                        "getBroadcastAddresses(): unable to fetch network interface configuration");
                delete[] pIfreqList;
                addDefaultBroadcastAddress(retVector, defaultPort);
                return retVector;
            }

            int maxNodes = ifconf.ifc_len/sizeof(ifreq);
            //errlogPrintf("Found %d interfaces\n", maxNodes);

            pifreq = pIfreqList;

            for(int i = 0; i<maxNodes; i++) {
                if(!(*pifreq->ifr_name)) break;

                if(i>0) {
                    size_t n = SA_LEN(pifreq)+sizeof(pifreq->ifr_name);
                    if(n<sizeof(ifreq))
                        pifreq++;
                    else
                        pifreq = (struct ifreq *)((char *)pifreq+n);
                }

                /*
                 * If its not an internet interface then dont use it
                 */
                if(pifreq->ifr_addr.sa_family!=AF_INET) continue;

                strncpy(ifrBuff.ifr_name, pifreq->ifr_name,
                        sizeof(ifrBuff.ifr_name));
                status = ioctl(sock, SIOCGIFFLAGS, &ifrBuff);
                if(status) {
                    errlogSevPrintf(
                            errlogMinor,
                            "getBroadcastAddresses(): net intf flags fetch for \"%s\" failed",
                            pifreq->ifr_name);
                    continue;
                }

                /*
                 * dont bother with interfaces that have been disabled
                 */
                if(!(ifrBuff.ifr_flags&IFF_UP)) continue;

                /*
                 * dont use the loop back interface
                 */
                if(ifrBuff.ifr_flags&IFF_LOOPBACK) continue;

                pNewNode = new osiSockAddr;
                if(pNewNode==NULL) {
                    errlogSevPrintf(errlogMajor,
                            "getBroadcastAddresses(): no memory available for configuration");
                    delete[] pIfreqList;
                    if(retVector->size()==0) addDefaultBroadcastAddress(
                            retVector, defaultPort);
                    return retVector;
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
                if(ifrBuff.ifr_flags&IFF_BROADCAST) {
                    strncpy(ifrBuff.ifr_name, pifreq->ifr_name,
                            sizeof(ifrBuff.ifr_name));
                    status = ioctl(sock, SIOCGIFBRDADDR, &ifrBuff);
                    if(status) {
                        errlogSevPrintf(
                                errlogMinor,
                                "getBroadcastAddresses(): net intf \"%s\": bcast addr fetch fail",
                                pifreq->ifr_name);
                        delete pNewNode;
                        continue;
                    }
                    pNewNode->sa = ifrBuff.ifr_broadaddr;
                }
#ifdef IFF_POINTOPOINT
                else if(ifrBuff.ifr_flags&IFF_POINTOPOINT) {
                    strncpy(ifrBuff.ifr_name, pifreq->ifr_name,
                            sizeof(ifrBuff.ifr_name));
                    status = ioctl(sock, SIOCGIFDSTADDR, &ifrBuff);
                    if(status) {
                        errlogSevPrintf(
                                errlogMinor,
                                "getBroadcastAddresses(): net intf \"%s\": pt to pt addr fetch fail",
                                pifreq->ifr_name);
                        delete pNewNode;
                        continue;
                    }
                    pNewNode->sa = ifrBuff.ifr_dstaddr;
                }
#endif
                else {
                    errlogSevPrintf(
                            errlogMinor,
                            "getBroadcastAddresses(): net intf \"%s\": not point to point or bcast?",
                            pifreq->ifr_name);
                    delete pNewNode;
                    continue;
                }
                pNewNode->ia.sin_port = htons(defaultPort);

                retVector->push_back(pNewNode);
            }

            delete[] pIfreqList;

            return retVector;
        }

        void encodeAsIPv6Address(ByteBuffer* buffer, const osiSockAddr* address) {
            // IPv4 compatible IPv6 address
            // first 80-bit are 0
            buffer->putLong(0);
            buffer->putShort(0);
            // next 16-bits are 1
            buffer->putShort(0xFFFF);
            // following IPv4 address in big-endian (network) byte order
            in_addr_t ipv4Addr = ntohl(address->ia.sin_addr.s_addr);
            buffer->putByte((int8)((ipv4Addr>>24)&0xFF));
            buffer->putByte((int8)((ipv4Addr>>16)&0xFF));
            buffer->putByte((int8)((ipv4Addr>>8)&0xFF));
            buffer->putByte((int8)(ipv4Addr&0xFF));
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

        int32 parseInetAddress(const String addr) {
            int32 retAddr;

            size_t dot = addr.find('.');
            if(dot==String::npos) THROW_BASE_EXCEPTION("Not an IPv4 address.");
            int byte = atoi(addr.substr(0, dot).c_str());
            if(byte<0||byte>255) THROW_BASE_EXCEPTION("Not an IPv4 address.");
            retAddr = byte;

            int num = dot+1;
            dot = addr.find('.', num);
            if(dot==String::npos) THROW_BASE_EXCEPTION("Not an IPv4 address.");
            byte = atoi(addr.substr(num, dot-num).c_str());
            if(byte<0||byte>255) THROW_BASE_EXCEPTION("Not an IPv4 address.");
            retAddr <<= 8;
            retAddr |= byte;

            num = dot+1;
            dot = addr.find('.', num);
            if(dot==String::npos) THROW_BASE_EXCEPTION("Not an IPv4 address.");
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

        InetAddrVector* getSocketAddressList(String list, int defaultPort,
                const InetAddrVector* appendList) {
            InetAddrVector* iav = new InetAddrVector();

            // parse string
            size_t subStart = 0;
            size_t subEnd;
            while((subEnd = list.find(' ', subStart))!=String::npos) {
                String address = list.substr(subStart, (subEnd-subStart));
                osiSockAddr* addr = new osiSockAddr;
                aToIPAddr(address.c_str(), defaultPort, &addr->ia);
                iav->push_back(addr);
                subStart = list.find_first_not_of(" \t\r\n\v", subEnd);
            }

            if(subStart!=String::npos&&list.length()>0) {
                osiSockAddr* addr = new osiSockAddr;
                aToIPAddr(list.substr(subStart).c_str(), defaultPort, &addr->ia);
                iav->push_back(addr);
            }

            if(appendList!=NULL) {
                for(size_t i = 0; i<appendList->size(); i++)
                    iav->push_back(appendList->at(i));
            }
            return iav;
        }

        const String inetAddressToString(const osiSockAddr *addr,
                bool displayPort, bool displayHex) {
            stringstream saddr;

            int ipa = ntohl(addr->ia.sin_addr.s_addr);

            saddr<<((int)(ipa>>24)&0xFF)<<'.';
            saddr<<((int)(ipa>>16)&0xFF)<<'.';
            saddr<<((int)(ipa>>8)&0xFF)<<'.';
            saddr<<((int)ipa&0xFF);
            if(displayPort) saddr<<":"<<ntohs(addr->ia.sin_port);
            if(displayHex) saddr<<" ("<<hex<<ntohl(addr->ia.sin_addr.s_addr)
                    <<")";

            return saddr.str();
        }

    }
}
