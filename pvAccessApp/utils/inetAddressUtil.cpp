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

/* standard */
#include <vector>
#include <cstring>
#include <cstdlib>
#include <sstream>

using namespace std;
using namespace epics::pvData;

namespace epics {
    namespace pvAccess {

        /* copied from EPICS v3 ca/iocinf.cpp
         * removeDuplicateAddresses ()
         */
        void removeDuplicateAddresses(ELLLIST *pDestList, ELLLIST *pSrcList,
                int silent) {
            ELLNODE *pRawNode;

            while((pRawNode = ellGet(pSrcList))) {
                STATIC_ASSERT(offsetof(osiSockAddrNode, node)==0);
                osiSockAddrNode *pNode =
                        reinterpret_cast<osiSockAddrNode *> (pRawNode);
                osiSockAddrNode *pTmpNode;

                if(pNode->addr.sa.sa_family==AF_INET) {

                    pTmpNode = (osiSockAddrNode *)ellFirst (pDestList); // X aCC 749
                    while(pTmpNode) {
                        if(pTmpNode->addr.sa.sa_family==AF_INET) {
                            if(pNode->addr.ia.sin_addr.s_addr
                                    ==pTmpNode->addr.ia.sin_addr.s_addr
                                    &&pNode->addr.ia.sin_port
                                            ==pTmpNode->addr.ia.sin_port) {
                                if(!silent) {
                                    char buf[64];
                                    ipAddrToDottedIP(&pNode->addr.ia, buf,
                                            sizeof(buf));
                                    fprintf(
                                            stderr,
                                            "Warning: Duplicate EPICS CA Address list entry \"%s\" discarded\n",
                                            buf);
                                }
                                free(pNode);
                                pNode = NULL;
                                break;
                            }
                        }
                        pTmpNode = (osiSockAddrNode *)ellNext (&pTmpNode->node); // X aCC 749
                    }
                    if(pNode) {
                        ellAdd(pDestList, &pNode->node);
                    }
                }
                else {
                    ellAdd(pDestList, &pNode->node);
                }
            }
        }

        InetAddrVector* getBroadcastAddresses(SOCKET sock) {
            ELLLIST bcastList;
            ELLLIST tmpList;
            osiSockAddr addr;

            ellInit ( &bcastList ); // X aCC 392
            ellInit ( &tmpList ); // X aCC 392

            addr.ia.sin_family = AF_UNSPEC;
            osiSockDiscoverBroadcastAddresses(&bcastList, sock, &addr);
            removeDuplicateAddresses(&tmpList, &bcastList, 1);
            // forcePort ( &bcastList, port );  // if needed copy from ca/iocinf.cpp

            int size = ellCount(&bcastList );
            InetAddrVector* retVector = new InetAddrVector(size);

            ELLNODE *pRawNode;

            while((pRawNode = ellGet(&tmpList))) {
                osiSockAddrNode *pNode =
                        reinterpret_cast<osiSockAddrNode *> (pRawNode);
                osiSockAddr* posa = new osiSockAddr;
                memcpy(posa, &(pNode->addr), sizeof(osiSockAddr));
                retVector->push_back(posa);
                free(pNode); // using free because it is allocated by calloc
            }

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
            in_addr_t ipv4Addr = address->ia.sin_addr.s_addr;
            buffer->putByte((int8)((ipv4Addr>>24)&0xFF));
            buffer->putByte((int8)((ipv4Addr>>16)&0xFF));
            buffer->putByte((int8)((ipv4Addr>>8)&0xFF));
            buffer->putByte((int8)(ipv4Addr&0xFF));
        }

        osiSockAddr* intToIPv4Address(int32 addr) {
            osiSockAddr* ret = new osiSockAddr;
            ret->ia.sin_family = AF_INET;
            ret->ia.sin_addr.s_addr = (in_addr_t)addr;
            ret->ia.sin_port = 0;

            return ret;
        }

        int32 ipv4AddressToInt(const osiSockAddr& addr) {
            return (int32)(addr.ia.sin_addr.s_addr);
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

            return retAddr;
        }

        osiSockAddr* processAddressForList(String address, int defaultPort) {
            // check port
            int port = defaultPort;
            size_t pos = address.find(':');
            if(pos!=String::npos) {
                port = atoi(address.substr(pos+1).c_str());
                address = address.substr(0, pos);
            }

            // add parsed address
            osiSockAddr* addr = new osiSockAddr;
            addr->ia.sin_family = AF_INET;
            addr->ia.sin_port = port;
            addr->ia.sin_addr.s_addr = parseInetAddress(address);

            return addr;
        }

        InetAddrVector* getSocketAddressList(String list, int defaultPort,
                const InetAddrVector* appendList) {
            InetAddrVector* iav = new InetAddrVector();

            // parse string
            size_t subStart = 0;
            size_t subEnd;
            while((subEnd = list.find(' ', subStart))!=String::npos) {
                String address = list.substr(subStart, (subEnd-subStart));

                iav->push_back(processAddressForList(address, defaultPort));
                subStart = list.find_first_not_of(" \t\r\n\v", subEnd);
            }

            if(subStart!=String::npos&&list.length()>0) iav->push_back(
                    processAddressForList(list.substr(subStart), defaultPort));

            if(appendList!=NULL) {
                for(size_t i = 0; i<appendList->size(); i++)
                    iav->push_back(appendList->at(i));
            }
            return iav;
        }

        const String inetAddressToString(const osiSockAddr *addr,
                bool displayHex) {
            stringstream saddr;

            saddr<<(int)((addr->ia.sin_addr.s_addr)>>24)<<'.';
            saddr<<((int)((addr->ia.sin_addr.s_addr)>>16)&0xFF)<<'.';
            saddr<<((int)((addr->ia.sin_addr.s_addr)>>8)&0xFF)<<'.';
            saddr<<((int)(addr->ia.sin_addr.s_addr)&0xFF);
            if(addr->ia.sin_port>0) saddr<<":"<<addr->ia.sin_port;
            if(displayHex) saddr<<" ("<<hex<<((uint32_t)(
                    addr->ia.sin_addr.s_addr))<<")";

            return saddr.str();
        }

    }
}
