/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/inetAddressUtil.h>
#include <pv/byteBuffer.h>
#include <pv/epicsException.h>

#include <osiSock.h>
#include <ellLib.h>
#include <epicsAssert.h>
#include <pv/logger.h>

#include <vector>
#include <cstring>
#include <cstdlib>
#include <sstream>

using namespace std;
using namespace epics::pvData;

namespace epics {
namespace pvAccess {

void addDefaultBroadcastAddress(InetAddrVector* v, unsigned short p) {
    osiSockAddr pNewNode;
    pNewNode.ia.sin_family = AF_INET;
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
        v->push_back(sn->addr);
    }
    ellFree(&as);
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
        osiSockAddr addr;
        aToIPAddr(address.c_str(), defaultPort, &addr.ia);
        iav->push_back(addr);
        subStart = list.find_first_not_of(" \t\r\n\v", subEnd);
    }

    if(subStart!=String::npos&&list.length()>0) {
        osiSockAddr addr;
        aToIPAddr(list.substr(subStart).c_str(), defaultPort, &addr.ia);
        iav->push_back(addr);
    }

    if(appendList!=NULL) {
        for(size_t i = 0; i<appendList->size(); i++)
        	iav->push_back((*appendList)[i]);
    }
    return iav;
}

const String inetAddressToString(const osiSockAddr &addr,
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

}
}
