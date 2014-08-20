#include <epicsUnitTest.h>
#include <testMain.h>

#include <pv/inetAddressUtil.h>
#include <pv/logger.h>

#include <pv/byteBuffer.h>
#include <pv/pvType.h>

#include <epicsAssert.h>
#include <osiSock.h>

#include <iostream>
#include <cstring>

using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace std;

void test_getSocketAddressList()
{
    testDiag("Test getSocketAddressList()");

    auto_ptr<InetAddrVector> vec(getSocketAddressList("127.0.0.1   10.10.12.11:1234 192.168.3.4", 555));

    testOk1(static_cast<size_t>(3) == vec->size());

    osiSockAddr addr;
    addr = vec->at(0);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(555) == addr.ia.sin_port);
    testOk1(htonl(0x7F000001) == addr.ia.sin_addr.s_addr);
    testOk1("127.0.0.1:555" == inetAddressToString(addr));

    addr = vec->at(1);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(1234) == addr.ia.sin_port);
    testOk1(htonl(0x0A0A0C0B) == addr.ia.sin_addr.s_addr);
    testOk1("10.10.12.11:1234" == inetAddressToString(addr));

    addr = vec->at(2);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(555) == addr.ia.sin_port);
    testOk1(htonl(0xC0A80304) == addr.ia.sin_addr.s_addr);
    testOk1("192.168.3.4:555" == inetAddressToString(addr));
    
    
    

    auto_ptr<InetAddrVector> vec1(getSocketAddressList("172.16.55.160", 6789, vec.get()));
    
    testOk1(static_cast<size_t>(4) == vec1->size());

    addr = vec1->at(0);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(6789) == addr.ia.sin_port);
    testOk1(htonl(0xAC1037A0) == addr.ia.sin_addr.s_addr);
    testOk1("172.16.55.160:6789" == inetAddressToString(addr));

    addr = vec1->at(1);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(555) == addr.ia.sin_port);
    testOk1(htonl(0x7F000001) == addr.ia.sin_addr.s_addr);
    testOk1("127.0.0.1:555" == inetAddressToString(addr));

    addr = vec1->at(2);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(1234) == addr.ia.sin_port);
    testOk1(htonl(0x0A0A0C0B) == addr.ia.sin_addr.s_addr);
    testOk1("10.10.12.11:1234" == inetAddressToString(addr));

    addr = vec1->at(3);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(555) == addr.ia.sin_port);
    testOk1(htonl(0xC0A80304) == addr.ia.sin_addr.s_addr);
    testOk1("192.168.3.4:555" == inetAddressToString(addr));
    
}

void test_ipv4AddressToInt()
{
    testDiag("Test ipv4AddressToInt()");

    auto_ptr<InetAddrVector> vec(getSocketAddressList("127.0.0.1   10.10.12.11:1234 192.168.3.4", 555));

    testOk1(static_cast<size_t>(3) == vec->size());

    testOk1((int32)0x7F000001 == ipv4AddressToInt((vec->at(0))));
    testOk1((int32)0x0A0A0C0B == ipv4AddressToInt((vec->at(1))));
    testOk1((int32)0xC0A80304 == ipv4AddressToInt((vec->at(2))));
}

void test_intToIPv4Address()
{
    testDiag("Test intToIPv4Address()");

    auto_ptr<osiSockAddr> paddr(intToIPv4Address(0x7F000001));
    testOk1((uintptr_t)0 != (uintptr_t)paddr.get());
    testOk1(AF_INET == paddr->ia.sin_family);
    testOk1("127.0.0.1:0" == inetAddressToString(*paddr.get()));

    paddr.reset(intToIPv4Address(0x0A0A0C0B));
    testOk1((uintptr_t)0 != (uintptr_t)paddr.get());
    testOk1(AF_INET == paddr->ia.sin_family);
    testOk1("10.10.12.11:0" == inetAddressToString(*paddr.get()));
}

void test_encodeAsIPv6Address()
{
    testDiag("Test encodeAsIPv6Address()");

    auto_ptr<ByteBuffer> buff(new ByteBuffer(32, EPICS_ENDIAN_LITTLE));

    char src[] = { (char)0, (char)0, (char)0, (char)0, (char)0, (char)0,
            (char)0, (char)0, (char)0, (char)0, (char)0xFF, (char)0xFF,
            (char)0x0A, (char)0x0A, (char)0x0C, (char)0x0B };

    auto_ptr<osiSockAddr> paddr(intToIPv4Address(0x0A0A0C0B));
    testOk1((uintptr_t)0 != (uintptr_t)paddr.get());
    osiSockAddr addr = *paddr;
    
    encodeAsIPv6Address(buff.get(), &addr);
    testOk1(static_cast<size_t>(16) == buff->getPosition());
    
    testOk1(strncmp(buff->getArray(), src, 16) == 0);
}

void test_isMulticastAddress()
{
    testDiag("Test test_isMulticastAddress()");

    auto_ptr<InetAddrVector> vec(getSocketAddressList("127.0.0.1 255.255.255.255 0.0.0.0 224.0.0.0 239.255.255.255 235.3.6.3", 0));

    testOk1(static_cast<size_t>(6) == vec->size());

    testOk1(!isMulticastAddress(&vec->at(0)));
    testOk1(!isMulticastAddress(&vec->at(1)));
    testOk1(!isMulticastAddress(&vec->at(2)));
    testOk1(isMulticastAddress(&vec->at(3)));
    testOk1(isMulticastAddress(&vec->at(4)));
    testOk1(isMulticastAddress(&vec->at(5)));
}

void test_getBroadcastAddresses()
{
    testDiag("Test getBroadcastAddresses()");

    osiSockAttach();

    SOCKET socket = epicsSocketCreate(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    auto_ptr<InetAddrVector> broadcasts(getBroadcastAddresses(socket, 6678));
    // at least one is expected
    testOk1(static_cast<size_t>(0) < broadcasts->size());
    epicsSocketDestroy(socket);

    // debug
    for(size_t i = 0; i<broadcasts->size(); i++) {
        testDiag("%s", inetAddressToString(broadcasts->at(i)).c_str());
    }

}

void test_getLoopbackNIF()
{
    testDiag("Test getLoopbackNIF()");

    osiSockAddr addr;
    unsigned short port = 5555;

    int defaultValue = getLoopbackNIF(addr, "", port);

    testOk1(defaultValue);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(port) == addr.ia.sin_port);
    testOk1(htonl(INADDR_LOOPBACK) == addr.ia.sin_addr.s_addr);

    defaultValue = getLoopbackNIF(addr, "10.0.0.1:7777", port);

    testOk1(!defaultValue);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(7777) == addr.ia.sin_port);
    testOk1(htonl(0x0A000001) == addr.ia.sin_addr.s_addr);
}

#ifdef _WIN32
// needed for ip_mreq
#include <Ws2tcpip.h>
#endif

void test_multicast()
{
    testDiag("Test test_multicast()");

    osiSockAttach();

    SOCKET socket = epicsSocketCreate(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    testOk1(socket != INVALID_SOCKET);
    if (socket != INVALID_SOCKET)
        return;
/*
    // set SO_REUSEADDR or SO_REUSEPORT, OS dependant
    epicsSocketEnableAddressUseForDatagramFanout(socket);

    osiSockAddr bindAddr;
    bindAddr.ia.sin_family = AF_INET;
    bindAddr.ia.sin_port = ntohs(5555);
    bindAddr.ia.sin_addr.s_addr = htonl(INADDR_ANY);

    int status = ::bind(socket, (sockaddr*)&(bindAddr.sa), sizeof(sockaddr));
    if (status)
    {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        fprintf(stderr, "Failed to bind: %s", errStr);
        epicsSocketDestroy(socket);
        return;
    }
*/
    struct ip_mreq imreq;
    memset(&imreq, 0, sizeof(struct ip_mreq));

    imreq.imr_multiaddr.s_addr = inet_addr("224.0.0.1");
    imreq.imr_interface.s_addr = INADDR_ANY;

       // join multicast group on default interface
    int status = ::setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                    (const void *)&imreq, sizeof(struct ip_mreq));
    if (status)
    {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        fprintf(stderr, "Error setting IP_ADD_MEMBERSHIP: %s", errStr);
    }
    testOk1(status == 0);

    epicsSocketDestroy(socket);
}

MAIN(testInetAddressUtils)
{
    testPlan(60);
    testDiag("Tests for InetAddress utils");

    test_getSocketAddressList();
    test_ipv4AddressToInt();
    test_intToIPv4Address();
    test_encodeAsIPv6Address();
    test_isMulticastAddress();
    test_getBroadcastAddresses();
    test_getLoopbackNIF();

    test_multicast();

    return testDone();
}
