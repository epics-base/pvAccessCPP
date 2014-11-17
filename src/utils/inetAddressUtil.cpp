/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <vector>
#include <cstring>
#include <cstdlib>
#include <sstream>

#include <pv/byteBuffer.h>
#include <pv/epicsException.h>
#include <osiSock.h>
#include <ellLib.h>

#define epicsExportSharedSymbols
#include <pv/inetAddressUtil.h>

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
        // TODO discover possible duplicates
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

}
}
