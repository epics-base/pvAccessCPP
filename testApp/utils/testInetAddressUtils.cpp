#include <epicsUnitTest.h>
#include <testMain.h>

#include <pv/pvUnitTest.h>

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

std::ostream& operator<<(std::ostream& strm, const osiSockAddr& addr)
{
    char buf[32];
    ipAddrToDottedIP(&addr.ia, buf, sizeof(buf));
    strm<<buf;
    return strm;
}

namespace {

void test_getSocketAddressList()
{
    testDiag("Test getSocketAddressList()");

    InetAddrVector vec;
    getSocketAddressList(vec, "127.0.0.1   10.10.12.11:1234 192.168.3.4", 555);

    testOk1(static_cast<size_t>(3) == vec.size());

    osiSockAddr addr;
    osiSockAddr addr2;
    addr = vec.at(0);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(555) == addr.ia.sin_port);
    testOk1(htonl(0x7F000001) == addr.ia.sin_addr.s_addr);
    testOk1("127.0.0.1:555" == inetAddressToString(addr));
    testOk1(stringToInetAddress("127.0.0.1:555", addr2));
    testOk1(AF_INET == addr2.ia.sin_family);
    testOk1(htons(555) == addr2.ia.sin_port);
    testOk1(htonl(0x7F000001) == addr2.ia.sin_addr.s_addr);
    testOk1(sockAddrAreIdentical(&addr2, &addr));

    addr = vec.at(1);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(1234) == addr.ia.sin_port);
    testOk1(htonl(0x0A0A0C0B) == addr.ia.sin_addr.s_addr);
    testOk1("10.10.12.11:1234" == inetAddressToString(addr));
    testOk1(stringToInetAddress("10.10.12.11:1234", addr2));
    testOk1(AF_INET == addr2.ia.sin_family);
    testOk1(htons(1234) == addr2.ia.sin_port);
    testOk1(htonl(0x0A0A0C0B) == addr2.ia.sin_addr.s_addr);
    testOk1(sockAddrAreIdentical(&addr2, &addr));

    addr = vec.at(2);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(555) == addr.ia.sin_port);
    testOk1(htonl(0xC0A80304) == addr.ia.sin_addr.s_addr);
    testOk1("192.168.3.4:555" == inetAddressToString(addr));
    testOk1(stringToInetAddress("192.168.3.4:555", addr2));
    testOk1(AF_INET == addr2.ia.sin_family);
    testOk1(htons(555) == addr2.ia.sin_port);
    testOk1(htonl(0xC0A80304) == addr2.ia.sin_addr.s_addr);
    testOk1(sockAddrAreIdentical(&addr2, &addr));


    InetAddrVector vec1;
    getSocketAddressList(vec1, "172.16.55.160", 6789, &vec);

    testOk1(static_cast<size_t>(4) == vec1.size());

    addr = vec1.at(0);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(6789) == addr.ia.sin_port);
    testOk1(htonl(0xAC1037A0) == addr.ia.sin_addr.s_addr);
    testOk1("172.16.55.160:6789" == inetAddressToString(addr));

    addr = vec1.at(1);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(555) == addr.ia.sin_port);
    testOk1(htonl(0x7F000001) == addr.ia.sin_addr.s_addr);
    testOk1("127.0.0.1:555" == inetAddressToString(addr));

    addr = vec1.at(2);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(1234) == addr.ia.sin_port);
    testOk1(htonl(0x0A0A0C0B) == addr.ia.sin_addr.s_addr);
    testOk1("10.10.12.11:1234" == inetAddressToString(addr));

    addr = vec1.at(3);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(555) == addr.ia.sin_port);
    testOk1(htonl(0xC0A80304) == addr.ia.sin_addr.s_addr);
    testOk1("192.168.3.4:555" == inetAddressToString(addr));


    // empty
    InetAddrVector vec2;
    getSocketAddressList(vec2, "", 1111);
    testOk1(static_cast<size_t>(0) == vec2.size());

    // just spaces
    InetAddrVector vec3;
    getSocketAddressList(vec3, "   ", 1111);
    testOk1(static_cast<size_t>(0) == vec3.size());

    // leading spaces
    InetAddrVector vec4;
    getSocketAddressList(vec4, "     127.0.0.1   10.10.12.11:1234 192.168.3.4", 555);

    testOk1(static_cast<size_t>(3) == vec4.size());

    addr = vec4.at(0);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(555) == addr.ia.sin_port);
    testOk1(htonl(0x7F000001) == addr.ia.sin_addr.s_addr);
    testOk1("127.0.0.1:555" == inetAddressToString(addr));

    addr = vec4.at(1);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(1234) == addr.ia.sin_port);
    testOk1(htonl(0x0A0A0C0B) == addr.ia.sin_addr.s_addr);
    testOk1("10.10.12.11:1234" == inetAddressToString(addr));

    addr = vec4.at(2);
    testOk1(AF_INET == addr.ia.sin_family);
    testOk1(htons(555) == addr.ia.sin_port);
    testOk1(htonl(0xC0A80304) == addr.ia.sin_addr.s_addr);
    testOk1("192.168.3.4:555" == inetAddressToString(addr));
}

void test_encodeAsIPv6Address()
{
    testDiag("Test encodeAsIPv6Address()");

    epics::auto_ptr<ByteBuffer> buff(new ByteBuffer(32, EPICS_ENDIAN_LITTLE));

    char src[] = { (char)0, (char)0, (char)0, (char)0, (char)0, (char)0,
                   (char)0, (char)0, (char)0, (char)0, (char)0xFF, (char)0xFF,
                   (char)0x0A, (char)0x0A, (char)0x0C, (char)0x0B
                 };

    osiSockAddr addr;
    memset(&addr, 0, sizeof(addr));
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl(0x0A0A0C0B);

    encodeAsIPv6Address(buff.get(), &addr);
    testOk1(static_cast<size_t>(16) == buff->getPosition());

    testOk1(strncmp(buff->getBuffer(), src, 16) == 0);
}

void test_isMulticastAddress()
{
    testDiag("Test test_isMulticastAddress()");

    InetAddrVector vec;
    getSocketAddressList(vec, "127.0.0.1 255.255.255.255 0.0.0.0 224.0.0.0 239.255.255.255 235.3.6.3", 0);

    testOk1(static_cast<size_t>(6) == vec.size());

    testOk1(!isMulticastAddress(&vec.at(0)));
    testOk1(!isMulticastAddress(&vec.at(1)));
    testOk1(!isMulticastAddress(&vec.at(2)));
    testOk1(isMulticastAddress(&vec.at(3)));
    testOk1(isMulticastAddress(&vec.at(4)));
    testOk1(isMulticastAddress(&vec.at(5)));
}

#ifdef _WIN32
// needed for ip_mreq
#include <ws2tcpip.h>
#endif

void test_multicastLoopback()
{
    testDiag("Test test_multicast()");

    osiSockAttach();

    SOCKET socket = epicsSocketCreate(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    testOk1(socket != INVALID_SOCKET);
    if (socket == INVALID_SOCKET)
        testAbort("Can't allocate socket");

    unsigned short port = 5555;

    // set SO_REUSEADDR or SO_REUSEPORT, OS dependant
    epicsSocketEnableAddressUseForDatagramFanout(socket);

    osiSockAddr bindAddr;
    memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.ia.sin_family = AF_INET;
    bindAddr.ia.sin_port = ntohs(port);
    bindAddr.ia.sin_addr.s_addr = htonl(INADDR_ANY);

    int status = ::bind(socket, (sockaddr*)&(bindAddr.sa), sizeof(sockaddr));
    if (status)
    {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        testFail("Failed to bind: %s\n", errStr);
        epicsSocketDestroy(socket);
        return;
    }

    osiSockAddr loAddr;
    memset(&loAddr, 0, sizeof(loAddr));
    loAddr.ia.sin_family = AF_INET;
    loAddr.ia.sin_port = ntohs(port);
    loAddr.ia.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    osiSockAddr mcastAddr;
    aToIPAddr("224.0.0.128", port, &mcastAddr.ia);

    struct ip_mreq imreq;
    memset(&imreq, 0, sizeof(struct ip_mreq));

    imreq.imr_multiaddr.s_addr = mcastAddr.ia.sin_addr.s_addr;
    imreq.imr_interface.s_addr = loAddr.ia.sin_addr.s_addr;

    // join multicast group on default interface
    status = ::setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                          (char*)&imreq, sizeof(struct ip_mreq));
    if (status)
    {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        testFail("Error setting IP_ADD_MEMBERSHIP: %s\n", errStr);
    }
    testOk(status == 0, "IP_ADD_MEMBERSHIP set");



    SOCKET sendSocket = epicsSocketCreate(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    testOk1(sendSocket != INVALID_SOCKET);
    if (sendSocket == INVALID_SOCKET)
        return;

    // set the multicast outgoing interface
    status = ::setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_IF,
                          (char*)&loAddr.ia.sin_addr, sizeof(struct in_addr));
    if (status)
    {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        testFail("Error setting IP_MULTICAST_IF: %s\n", errStr);
    }
    testOk(status == 0, "IP_MULTICAST_IF set");

    // send multicast traffic to myself too
    unsigned char mcast_loop = 1;
    status = ::setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_LOOP,
                          (char*)&mcast_loop, sizeof(unsigned char));
    if (status)
    {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        testFail("Error setting IP_MULTICAST_LOOP: %s\n", errStr);
    }
    testOk(status == 0, "IP_MULTICAST_LOOP set");

    // put some data in buffer
#define MAX_BUFFER_SIZE 1024
    char txbuff[MAX_BUFFER_SIZE];
    strcpy(txbuff, "mcastTest");

    // send multicast packet
    size_t len = strlen(txbuff);
    status = ::sendto(sendSocket, txbuff, len, 0,
                      &(mcastAddr.sa), sizeof(sockaddr));
    if (status < 0)
    {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        testFail("Multicast send error: %s\n", errStr);
    }
    testOk((size_t)status == len, "Multicast send");


    // set timeout in case message is not sent
    struct timeval timeout;
    memset(&timeout, 0, sizeof(struct timeval));
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    status = ::setsockopt (socket, SOL_SOCKET, SO_RCVTIMEO,
                           (char*)&timeout, sizeof(timeout));
    if (status)
    {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        testFail("Error setting SO_RCVTIMEO: %s\n", errStr);
    }
    testOk(status == 0, "SO_RCVTIMEO set");

    char rxbuff[MAX_BUFFER_SIZE];

    osiSockAddr fromAddress;
    osiSocklen_t addrStructSize = sizeof(sockaddr);

    // receive packet from socket
    status = ::recvfrom(socket, rxbuff, MAX_BUFFER_SIZE, 0,
                        (sockaddr*)&fromAddress, &addrStructSize);
    if (status < 0)
    {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        testFail("Multicast recv error: %s\n", errStr);
    }
    testOk((size_t)status == len, "Multicast recv");
    testOk(strncmp(rxbuff, txbuff, len) == 0, "Multicast content matches");

    // shutdown sockets?
    epicsSocketDestroy(sendSocket);
    epicsSocketDestroy(socket);
}

void test_discoverInterfaces()
{
    testDiag("test_discoverInterfaces()");

    SOCKET sock(epicsSocketCreate(AF_INET, SOCK_DGRAM, 0));
    if(sock==INVALID_SOCKET)
        testAbort("Failed to allocate socket");

    IfaceNodeVector ifaces;

    osiSockAddr any;
    memset(&any, 0, sizeof(any));
    any.ia.sin_family = AF_INET;
    any.ia.sin_addr.s_addr = htonl(INADDR_ANY);

    testEqual(discoverInterfaces(ifaces, sock, &any), 0);
    testOk(ifaces.size()>0u, "Found %u interfaces", unsigned(ifaces.size()));

    for(size_t i=0; i<ifaces.size(); i++)
    {
        const ifaceNode& node = ifaces[i];
        testShow()<<"Iface["<<i<<"] addr="<<node.addr;
        if(node.validP2P) {
            testShow()<<"  peer="<<node.peer;
        }
        if(node.validBcast) {
            testShow()<<"  mask="<<node.mask<<" bcast="<<node.bcast;

            epicsUInt32 ip   =ntohl(node.addr.ia.sin_addr.s_addr),
                        mask =ntohl(node.mask.ia.sin_addr.s_addr),
                        bcast=ntohl(node.bcast.ia.sin_addr.s_addr),
                        net  =ip&mask,
                        bcast2=net|~mask;

            testDiag("IP %08x/%08x Bcast %08x == %08x", ip, mask, bcast, bcast2);
        }
        if(node.loopback) {
            testShow()<<"  loopback";
        }
    }
}

} // namespace

MAIN(testInetAddressUtils)
{
    testPlan(80);
    testDiag("Tests for InetAddress utils");

    test_getSocketAddressList();
    test_encodeAsIPv6Address();
    test_isMulticastAddress();

    test_multicastLoopback();
    test_discoverInterfaces();

    return testDone();
}
