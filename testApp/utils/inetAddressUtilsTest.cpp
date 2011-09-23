#include <gtest/gtest.h>

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

TEST(inetAddressUtils, getSocketAddressList)
{
    auto_ptr<InetAddrVector> vec(getSocketAddressList("127.0.0.1   10.10.12.11:1234 192.168.3.4", 555));

    ASSERT_EQ(static_cast<size_t>(3), vec->size());

    osiSockAddr addr;
    addr = vec->at(0);
    EXPECT_EQ(AF_INET, addr.ia.sin_family);
    EXPECT_EQ(htons(555), addr.ia.sin_port);
    EXPECT_EQ(htonl(0x7F000001), addr.ia.sin_addr.s_addr);
    EXPECT_EQ("127.0.0.1:555", inetAddressToString(addr));

    addr = vec->at(1);
    EXPECT_EQ(AF_INET, addr.ia.sin_family);
    EXPECT_EQ(htons(1234), addr.ia.sin_port);
    EXPECT_EQ(htonl(0x0A0A0C0B), addr.ia.sin_addr.s_addr);
    EXPECT_EQ("10.10.12.11:1234", inetAddressToString(addr));

    addr = vec->at(2);
    EXPECT_EQ(AF_INET, addr.ia.sin_family);
    EXPECT_EQ(htons(555), addr.ia.sin_port);
    EXPECT_EQ(htonl(0xC0A80304), addr.ia.sin_addr.s_addr);
    EXPECT_EQ("192.168.3.4:555", inetAddressToString(addr));
    
    
    

    auto_ptr<InetAddrVector> vec1(getSocketAddressList("172.16.55.160", 6789, vec.get()));
    
    ASSERT_EQ(static_cast<size_t>(4), vec1->size());

    addr = vec1->at(0);
    EXPECT_EQ(AF_INET, addr.ia.sin_family);
    EXPECT_EQ(htons(6789), addr.ia.sin_port);
    EXPECT_EQ(htonl(0xAC1037A0), addr.ia.sin_addr.s_addr);
    EXPECT_EQ("172.16.55.160:6789", inetAddressToString(addr));

    addr = vec1->at(1);
    EXPECT_EQ(AF_INET, addr.ia.sin_family);
    EXPECT_EQ(htons(555), addr.ia.sin_port);
    EXPECT_EQ(htonl(0x7F000001), addr.ia.sin_addr.s_addr);
    EXPECT_EQ("127.0.0.1:555", inetAddressToString(addr));

    addr = vec1->at(2);
    EXPECT_EQ(AF_INET, addr.ia.sin_family);
    EXPECT_EQ(htons(1234), addr.ia.sin_port);
    EXPECT_EQ(htonl(0x0A0A0C0B), addr.ia.sin_addr.s_addr);
    EXPECT_EQ("10.10.12.11:1234", inetAddressToString(addr));

    addr = vec1->at(3);
    EXPECT_EQ(AF_INET, addr.ia.sin_family);
    EXPECT_EQ(htons(555), addr.ia.sin_port);
    EXPECT_EQ(htonl(0xC0A80304), addr.ia.sin_addr.s_addr);
    EXPECT_EQ("192.168.3.4:555", inetAddressToString(addr));
    
}

TEST(inetAddressUtils, ipv4AddressToInt)
{
    auto_ptr<InetAddrVector> vec(getSocketAddressList("127.0.0.1   10.10.12.11:1234 192.168.3.4", 555));

    ASSERT_EQ(static_cast<size_t>(3), vec->size());

    EXPECT_EQ((int32)0x7F000001, ipv4AddressToInt((vec->at(0))));
    EXPECT_EQ((int32)0x0A0A0C0B, ipv4AddressToInt((vec->at(1))));
    EXPECT_EQ((int32)0xC0A80304, ipv4AddressToInt((vec->at(2))));
}

TEST(inetAddressUtils, intToIPv4Address)
{
    auto_ptr<osiSockAddr> paddr(intToIPv4Address(0x7F000001));
    ASSERT_NE((uintptr_t)0, (uintptr_t)paddr.get());
    EXPECT_EQ(AF_INET, paddr->ia.sin_family);
    EXPECT_EQ("127.0.0.1:0", inetAddressToString(*paddr.get()));

    paddr.reset(intToIPv4Address(0x0A0A0C0B));
    ASSERT_NE((uintptr_t)0, (uintptr_t)paddr.get());
    EXPECT_EQ(AF_INET, paddr->ia.sin_family);
    EXPECT_EQ("10.10.12.11:0", inetAddressToString(*paddr.get()));
}


TEST(inetAddressUtils, encodeAsIPv6Address)
{
    auto_ptr<ByteBuffer> buff(new ByteBuffer(32, EPICS_ENDIAN_LITTLE));

    char src[] = { (char)0, (char)0, (char)0, (char)0, (char)0, (char)0,
            (char)0, (char)0, (char)0, (char)0, (char)0xFF, (char)0xFF,
            (char)0x0A, (char)0x0A, (char)0x0C, (char)0x0B };

    auto_ptr<osiSockAddr> paddr(intToIPv4Address(0x0A0A0C0B));
    ASSERT_NE((uintptr_t)0, (uintptr_t)paddr.get());
    osiSockAddr addr = *paddr;
    
    encodeAsIPv6Address(buff.get(), &addr);
    ASSERT_EQ(static_cast<uintptr_t>(16), buff->getPosition());
    
    EXPECT_TRUE(strncmp(buff->getArray(), src, 16)==0);
}



TEST(inetAddressUtils, getBroadcastAddresses)
{
    osiSockAttach();

    SOCKET socket = epicsSocketCreate(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    auto_ptr<InetAddrVector> broadcasts(getBroadcastAddresses(socket,6678));
    // at least one is expected
    ASSERT_LT(static_cast<size_t>(0), broadcasts->size());
    epicsSocketDestroy(socket);

    // debug
    for(size_t i = 0; i<broadcasts->size(); i++) {
        cout<<"\t"<<inetAddressToString(broadcasts->at(i))<<endl;
    }

}