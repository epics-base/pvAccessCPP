/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
#include <stdio.h>
#if !defined(_WIN32)
#include <signal.h>
#define USE_SIGNAL
#endif

#include <iostream>

#include <string>
#include <ostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

#include <pv/logger.h>
#include <pv/pvTimeStamp.h>
#include <pv/remote.h>
#include <pv/inetAddressUtil.h>
#include <pv/rpcClient.h>
#include <pv/logger.h>

#include "pvutils.h"

using namespace epics::pvData;
using namespace epics::pvAccess;

double timeout = 5.0;
bool debugFlag = false;

pvd::PVStructure::Formatter::format_t outmode = pvd::PVStructure::Formatter::NT;
int verbosity;

std::string request("");
std::string defaultProvider("pva");

epicsMutex Tracker::doneLock;
epicsEvent Tracker::doneEvt;
Tracker::inprog_t Tracker::inprog;
bool Tracker::abort = false;

#ifdef USE_SIGNAL
static
void alldone(int num)
{
    (void)num;
    Tracker::abort = true;
    Tracker::doneEvt.signal();
}
#endif

void Tracker::prepare()
{
#ifdef USE_SIGNAL
        signal(SIGINT, alldone);
        signal(SIGTERM, alldone);
        signal(SIGQUIT, alldone);
#endif
}

static
void early(const char *inp, unsigned pos)
{
    fprintf(stderr, "Unexpected end of input: %s\n", inp);
    throw std::runtime_error("Unexpected end of input");
}

// rudimentory parser for json array
// needed as long as Base < 3.15 is supported.
// for consistency, used with all version
void jarray(pvd::shared_vector<std::string>& out, const char *inp)
{
    assert(inp[0]=='[');
    const char * const orig = inp;
    inp++;

    while(true) {
        // starting a new token

        for(; *inp==' '; inp++) {} // skip leading whitespace

        if(*inp=='\0') early(inp, inp-orig);

        if(isalnum(*inp) || *inp=='+' || *inp=='-') {
            // number

            const char *start = inp;

            while(isalnum(*inp) || *inp=='.' || *inp=='+' || *inp=='-')
                inp++;

            if(*inp=='\0') early(inp, inp-orig);

            // inp points to first char after token

            out.push_back(std::string(start, inp-start));

        } else if(*inp=='"') {
            // quoted string

            const char *start = ++inp; // skip quote

            while(*inp!='\0' && *inp!='"')
                inp++;

            if(*inp=='\0') early(inp, inp-orig);

            // inp points to trailing "

            out.push_back(std::string(start, inp-start));

            inp++; // skip trailing "

        } else if(*inp==']') {
            // no-op
        } else {
            fprintf(stderr, "Unknown token '%c' in \"%s\"", *inp, inp);
            throw std::runtime_error("Unknown token");
        }

        for(; *inp==' '; inp++) {} // skip trailing whitespace

        if(*inp==',') inp++;
        else if(*inp==']') break;
        else {
            fprintf(stderr, "Unknown token '%c' in \"%s\"", *inp, inp);
            throw std::runtime_error("Unknown token");
        }
    }
}

// Get hex representation of byte.
std::string toHex(char ba[], size_t len)
{
    // Byte to hexchar mapping.
    static const char lookup[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };

    std::string sb;

    for (size_t i = 0; i < len; i++)
    {
        char b = ba[i];

        int upper = (b>>4)&0x0F;
        sb += lookup[upper];

        int lower = b&0x0F;
        sb += lookup[lower];
    }

    return sb;
}

// Read size
std::size_t readSize(ByteBuffer* buffer)
{
    int8 b = buffer->getByte();
    if(b==-1)
        return -1;
    else if(b==-2) {
        int32 s = buffer->getInt();
        if(s<0) THROW_BASE_EXCEPTION("negative size");
        return s;
    }
    else
        return (std::size_t)(b<0 ? b+256 : b);
}

// Deserialize string
std::string deserializeString(ByteBuffer* buffer)
{

    std::size_t size = /*SerializeHelper::*/readSize(buffer);
    if(size!=(size_t)-1)    // TODO null strings check, to be removed in the future
    {
        // entire string is in buffer, simply create a string out of it (copy)
        std::size_t pos = buffer->getPosition();
        std::string str(buffer->getBuffer()+pos, size);
        buffer->setPosition(pos+size);
        return str;
    }
    else
        return std::string();
}

// Process search response
// Returns true if new server response is received
bool processSearchResponse(const osiSockAddr& responseFrom, ByteBuffer& receiveBuffer, ServerMap& serverMapByGuid)
{
    // first byte is PVA_MAGIC
    int8 magic = receiveBuffer.getByte();
    if(magic != PVA_MAGIC) {
        return false;
    }

    // second byte version
    int8 version = receiveBuffer.getByte();
    if(version == 0) {
        // 0 -> 1 included incompatible changes
        return false;
    }

    // only data for UDP
    int8 flags = receiveBuffer.getByte();
    if (flags < 0) {
        // 7-bit set
        receiveBuffer.setEndianess(EPICS_ENDIAN_BIG);
    }
    else {
        receiveBuffer.setEndianess(EPICS_ENDIAN_LITTLE);
    }

    // command ID and paylaod
    int8 command = receiveBuffer.getByte();
    if (command != (int8)0x04) {
        return false;
    }

    size_t payloadSize = receiveBuffer.getInt();
    if (payloadSize < (12+4+16+2)) {
        return false;
    }

    ServerGUID guid;
    receiveBuffer.get(guid.value, 0, sizeof(guid.value));

    /*int32 searchSequenceId = */receiveBuffer.getInt();

    osiSockAddr serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.ia.sin_family = AF_INET;

    // 128-bit IPv6 address
    if (!decodeAsIPv6Address(&receiveBuffer, &serverAddress)) {
        return false;
    }

    // accept given address if explicitly specified by sender
    if (serverAddress.ia.sin_addr.s_addr == INADDR_ANY) {
        serverAddress.ia.sin_addr = responseFrom.ia.sin_addr;
    }

    // NOTE: htons might be a macro (e.g. vxWorks)
    int16 port = receiveBuffer.getShort();
    serverAddress.ia.sin_port = htons(port);
    std::string protocol = /*SerializeHelper::*/deserializeString(&receiveBuffer);

    /*bool found =*/ receiveBuffer.getByte(); // != 0;


    std::string guidString = toHex(guid.value, sizeof(guid.value));

    ServerMap::iterator iter = serverMapByGuid.find(guidString);
    if (iter != serverMapByGuid.end()) {
        bool found = false;
        std::vector<osiSockAddr>& vec = iter->second.addresses;
        for (std::vector<osiSockAddr>::const_iterator ai = vec.begin(); ai != vec.end(); ++ai) {
            if (sockAddrAreIdentical(&(*ai), &serverAddress)) {
                found = true;
                break;
            }
        }

        if (!found) {
            vec.push_back(serverAddress);
            return true;
        }
        else {
            return false;
        }
    }
    else {
        ServerEntry serverEntry;
        serverEntry.guid = guidString;
        serverEntry.protocol = protocol;
        serverEntry.addresses.push_back(serverAddress);
        serverEntry.version = version;
        serverMapByGuid[guidString] = serverEntry;
        return true;
    }
}

// Discover servers
bool discoverServers(double timeOut, ServerMap& serverMapByGuid)
{
    osiSockAttach();

    SOCKET socket = epicsSocketCreate(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == INVALID_SOCKET) {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        fprintf(stderr, "Failed to create a socket: %s\n", errStr);
        return false;
    }

    //
    // read config
    //

    Configuration::shared_pointer configuration(new SystemConfigurationImpl());

    std::string addressList = configuration->getPropertyAsString("EPICS_PVA_ADDR_LIST", "");
    bool autoAddressList = configuration->getPropertyAsBoolean("EPICS_PVA_AUTO_ADDR_LIST", true);
    int broadcastPort = configuration->getPropertyAsInteger("EPICS_PVA_BROADCAST_PORT", PVA_BROADCAST_PORT);

    // query broadcast addresses of all IFs
    InetAddrVector broadcastAddresses;
    {
        IfaceNodeVector ifaces;
        if(discoverInterfaces(ifaces, socket, 0)) {
            fprintf(stderr, "Unable to populate interface list\n");
            return false;
        }

        for(IfaceNodeVector::const_iterator it(ifaces.begin()), end(ifaces.end()); it!=end; ++it)
        {
            if(it->validBcast && it->bcast.sa.sa_family == AF_INET) {
                osiSockAddr bcast = it->bcast;
                bcast.ia.sin_port = htons(broadcastPort);
                broadcastAddresses.push_back(bcast);
            }
        }
    }

    // set broadcast address list
    if (!addressList.empty()) {
        // if auto is true, add it to specified list
        InetAddrVector* appendList = 0;
        if (autoAddressList)
            appendList = &broadcastAddresses;

        InetAddrVector list;
        getSocketAddressList(list, addressList, broadcastPort, appendList);
        if (!list.empty()) {
            // delete old list and take ownership of a new one
            broadcastAddresses = list;
        }
    }

    for (size_t i = 0; i < broadcastAddresses.size(); i++) {
        LOG(logLevelDebug, "Broadcast address #%d: %s.", int(i), inetAddressToString(broadcastAddresses[i]).c_str());
    }

    // ---

    int optval = 1;
    int status = ::setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
    if (status) {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        fprintf(stderr, "Error setting SO_BROADCAST: %s\n", errStr);
        epicsSocketDestroy (socket);
        return false;
    }

    osiSockAddr bindAddr;
    memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.ia.sin_family = AF_INET;
    bindAddr.ia.sin_port = htons(0);
    bindAddr.ia.sin_addr.s_addr = htonl(INADDR_ANY);

    status = ::bind(socket, static_cast<sockaddr*>(&bindAddr.sa), sizeof(sockaddr));
    if (status) {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        fprintf(stderr, "Failed to bind: %s\n", errStr);
        epicsSocketDestroy(socket);
        return false;
    }

    // set timeout
#ifdef _WIN32
    // ms
    DWORD timeout = 250;
#else
    struct timeval timeout;
    memset(&timeout, 0, sizeof(struct timeval));
    timeout.tv_sec = 0;
    timeout.tv_usec = 250000;
#endif
    status = ::setsockopt (socket, SOL_SOCKET, SO_RCVTIMEO,
                           &timeout, sizeof(timeout));
    if (status) {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        fprintf(stderr, "Error setting SO_RCVTIMEO: %s\n", errStr);
        return false;
    }

    osiSockAddr responseAddress;
    osiSocklen_t sockLen = sizeof(sockaddr);
    // read the actual socket info
    status = ::getsockname(socket, &responseAddress.sa, &sockLen);
    if (status) {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        fprintf(stderr, "Failed to get local socket address: %s.", errStr);
        return false;
    }

    char buffer[1024];
    ByteBuffer sendBuffer(buffer, sizeof(buffer)/sizeof(char));

    sendBuffer.putByte(PVA_MAGIC);
    sendBuffer.putByte(PVA_CLIENT_PROTOCOL_REVISION);
    sendBuffer.putByte((EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG) ? 0x80 : 0x00); // data + 7-bit endianess
    sendBuffer.putByte((int8_t)CMD_SEARCH); // search
    sendBuffer.putInt(4+1+3+16+2+1+2);      // "zero" payload

    sendBuffer.putInt(0);   // sequenceId
    sendBuffer.putByte((int8_t)0x81);    // reply required // TODO unicast vs multicast; for now we mark ourselves as unicast
    sendBuffer.putByte((int8_t)0);    // reserved
    sendBuffer.putShort((int16_t)0);  // reserved

    // NOTE: is it possible (very likely) that address is any local address ::ffff:0.0.0.0
    encodeAsIPv6Address(&sendBuffer, &responseAddress);
    sendBuffer.putShort((int16_t)ntohs(responseAddress.ia.sin_port));

    sendBuffer.putByte((int8_t)0x00);   // protocol count
    sendBuffer.putShort((int16_t)0);    // name count

    bool oneOK = false;
    for (size_t i = 0; i < broadcastAddresses.size(); i++) {
        if(pvAccessIsLoggable(logLevelDebug)) {
            char strBuffer[64];
            sockAddrToDottedIP(&broadcastAddresses[i].sa, strBuffer, sizeof(strBuffer));
            LOG(logLevelDebug, "UDP Tx (%lu) -> %s", sendBuffer.getPosition(), strBuffer);
        }

        status = ::sendto(socket, sendBuffer.getBuffer(), sendBuffer.getPosition(), 0,
                          &broadcastAddresses[i].sa, sizeof(sockaddr));
        if (status < 0) {
            char errStr[64];
            epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
            fprintf(stderr, "Send error: %s\n", errStr);
        }
        else {
            oneOK = true;
        }
    }

    if (!oneOK) {
        return false;
    }

    char rxbuff[1024];
    ByteBuffer receiveBuffer(rxbuff, sizeof(rxbuff)/sizeof(char));

    osiSockAddr fromAddress;
    osiSocklen_t addrStructSize = sizeof(sockaddr);

    int sendCount = 0;

    while (true) {
        receiveBuffer.clear();

        // receive packet from socket
        int bytesRead = ::recvfrom(socket, const_cast<char*>(receiveBuffer.getBuffer()),
                                   receiveBuffer.getRemaining(), 0,
                                   static_cast<sockaddr*>(&fromAddress.sa), &addrStructSize);

        if (bytesRead > 0) {
            if(pvAccessIsLoggable(logLevelDebug)) {
                char strBuffer[64];
                sockAddrToDottedIP(&fromAddress.sa, strBuffer, sizeof(strBuffer));
                LOG(logLevelDebug, "UDP Rx (%d) <- %s", bytesRead, strBuffer);
            }
            receiveBuffer.setPosition(bytesRead);
            receiveBuffer.flip();

            processSearchResponse(fromAddress, receiveBuffer, serverMapByGuid);

        }
        else {
            if (bytesRead == -1) {
                int socketError = SOCKERRNO;

                // interrupted or timeout
                if (socketError == SOCK_EINTR ||
                        socketError == EAGAIN ||        // no alias in libCom
                        // windows times out with this
                        socketError == SOCK_ETIMEDOUT ||
                        socketError == SOCK_EWOULDBLOCK) {
                    // OK
                }
                else if (socketError == SOCK_ECONNREFUSED || // avoid spurious ECONNREFUSED in Linux
                         socketError == SOCK_ECONNRESET) {   // or ECONNRESET in Windows
                    // OK
                }
                else {
                    // unexpected error
                    char errStr[64];
                    epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                    fprintf(stderr, "Socket recv error: %s\n", errStr);
                    break;
                }
            }

            if (++sendCount < 3) {
                // TODO duplicate code
                bool oneOK = false;
                for (size_t i = 0; i < broadcastAddresses.size(); i++) {
                    // send the packet
                    status = ::sendto(socket, sendBuffer.getBuffer(), sendBuffer.getPosition(), 0, &broadcastAddresses[i].sa, sizeof(sockaddr));
                    if (status < 0) {
                        char errStr[64];
                        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                        fprintf(stderr, "Send error: %s\n", errStr);
                    }
                    else {
                        oneOK = true;
                    }
                }

                if (!oneOK) {
                    return false;
                }
            }
            else {
                break;
            }
        }
    }

    // TODO shutdown sockets?
    // TODO this resouce is not released on failure
    epicsSocketDestroy(socket);

    return true;
}

PVStructure::shared_pointer getChannelInfo(const std::string& serverAddress, const std::string& queryOp, double timeOut)
{
    LOG(logLevelDebug, "Querying server %s for %s", serverAddress.c_str(), queryOp.c_str());
    StructureConstPtr argstype(getFieldCreate()->createFieldBuilder()
                                   ->setId("epics:nt/NTURI:1.0")
                                   ->add("scheme", pvString)
                                   ->add("path", pvString)
                                   ->addNestedStructure("query")
                                   ->add("op", pvString)
                                   ->endNested()
                                   ->createStructure());

    PVStructure::shared_pointer args(getPVDataCreate()->createPVStructure(argstype));

    args->getSubFieldT<PVString>("scheme")->put("pva");
    args->getSubFieldT<PVString>("path")->put("server");
    args->getSubFieldT<PVString>("query.op")->put(queryOp);

    PVStructure::shared_pointer ret;
    RPCClient rpc("server", createRequest("field()"), ChannelProvider::shared_pointer(), serverAddress);
    return rpc.request(args, timeOut, true);
}
