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

using namespace std;
using namespace epics::pvData;

namespace epics {
    namespace pvAccess {

        /* port of osiSockDiscoverBroadcastAddresses() in
         * epics/base/src/libCom/osi/os/default/osdNetIntf.c
         */
        InetAddrVector* getBroadcastAddresses(SOCKET sock) {
            static const unsigned nelem = 100;
            int status;
            struct ifconf ifconf;
            struct ifreq* pIfreqList;
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
                return retVector;
            }

            // get number of interfaces
            ifconf.ifc_len = nelem*sizeof(ifreq);
            ifconf.ifc_req = pIfreqList;
            status = ioctl(sock, SIOCGIFCONF, &ifconf);
            if(status<0||ifconf.ifc_len==0) {
                errlogSevPrintf(
                        errlogMinor,
                        "getBroadcastAddresses(): unable to fetch network interface configuration");
                delete[] pIfreqList;
                return retVector;
            }

            errlogPrintf("Found %d interfaces\n", ifconf.ifc_len);

            for(int i = 0; i<=ifconf.ifc_len; i++) {
                /*
                 * If its not an internet interface then dont use it
                 */
                if(pIfreqList[i].ifr_addr.sa_family!=AF_INET) continue;

                status = ioctl(sock, SIOCGIFFLAGS, &pIfreqList[i]);
                if(status) {
                    errlogSevPrintf(
                            errlogMinor,
                            "getBroadcastAddresses(): net intf flags fetch for \"%s\" failed",
                            pIfreqList[i].ifr_name);
                    continue;
                }

                /*
                 * dont bother with interfaces that have been disabled
                 */
                if(!(pIfreqList[i].ifr_flags&IFF_UP)) continue;

                /*
                 * dont use the loop back interface
                 */
                if(pIfreqList[i].ifr_flags&IFF_LOOPBACK) continue;

                pNewNode = new osiSockAddr;
                if(pNewNode==NULL) {
                    errlogSevPrintf(errlogMajor,
                            "getBroadcastAddresses(): no memory available for configuration");
                    delete[] pIfreqList;
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
                if(pIfreqList[i].ifr_flags&IFF_BROADCAST) {
                    status = ioctl(sock, SIOCGIFBRDADDR, &pIfreqList[i]);
                    if(status) {
                        errlogSevPrintf(
                                errlogMinor,
                                "getBroadcastAddresses(): net intf \"%s\": bcast addr fetch fail",
                                pIfreqList->ifr_name);
                        delete pNewNode;
                        continue;
                    }
                    pNewNode->sa = pIfreqList[i].ifr_broadaddr;
                }
#ifdef IFF_POINTOPOINT
                else if(pIfreqList->ifr_flags&IFF_POINTOPOINT) {
                    status = ioctl(sock, SIOCGIFDSTADDR, &pIfreqList[i]);
                    if(status) {
                        errlogSevPrintf(
                                errlogMinor,
                                "getBroadcastAddresses(): net intf \"%s\": pt to pt addr fetch fail",
                                pIfreqList[i].ifr_name);
                        delete pNewNode;
                        continue;
                    }
                    pNewNode->sa = pIfreqList[i].ifr_dstaddr;
                }
#endif
                else {
                    errlogSevPrintf(
                            errlogMinor,
                            "getBroadcastAddresses(): net intf \"%s\": not point to point or bcast?",
                            pIfreqList[i].ifr_name);
                    delete pNewNode;
                    continue;
                }

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
