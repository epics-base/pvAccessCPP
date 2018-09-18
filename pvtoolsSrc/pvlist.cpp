/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <stdio.h>

#include <iostream>
#include <map>
#include <iterator>
#include <vector>
#include <string>
#include <istream>
#include <fstream>
#include <sstream>


#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <pv/logger.h>

#include <epicsExit.h>
#include <osiSock.h>

#include <pv/byteBuffer.h>
#include <pv/serializeHelper.h>
#include <pv/pvaConstants.h>
#include <pv/inetAddressUtil.h>
#include <pv/configuration.h>
#include <pv/remote.h>
#include <pv/rpcClient.h>

#if defined(_WIN32) && !defined(_MINGW)
FILE *popen(const char *command, const char *mode) {
    return _popen(command, mode);
}
int pclose(FILE *stream) {
    return _pclose(stream);
}
#endif

using namespace std;

using namespace epics::pvData;
using namespace epics::pvAccess;

namespace {

/// Byte to hexchar mapping.
static const char lookup[] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

/// Get hex representation of byte.
string toHex(int8* ba, size_t len) {
    string sb;

    for (size_t i = 0; i < len; i++)
    {
        int8 b = ba[i];

        int upper = (b>>4)&0x0F;
        sb += lookup[upper];

        int lower = b&0x0F;
        sb += lookup[lower];
    }

    return sb;
}


std::size_t readSize(ByteBuffer* buffer) {
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

string deserializeString(ByteBuffer* buffer) {

    std::size_t size = /*SerializeHelper::*/readSize(buffer);
    if(size!=(size_t)-1)	// TODO null strings check, to be removed in the future
    {
        // entire string is in buffer, simply create a string out of it (copy)
        std::size_t pos = buffer->getPosition();
        string str(buffer->getArray()+pos, size);
        buffer->setPosition(pos+size);
        return str;
    }
    else
        return std::string();
}

struct ServerEntry {
    string guid;
    string protocol;
    vector<osiSockAddr> addresses;
    int8 version;
};


typedef map<string, ServerEntry> ServerMap;
static ServerMap serverMap;

// return true if new server response is recevived
bool processSearchResponse(osiSockAddr const & responseFrom, ByteBuffer & receiveBuffer)
{
    // first byte is PVA_MAGIC
    int8 magic = receiveBuffer.getByte();
    if(magic != PVA_MAGIC)
        return false;

    // second byte version
    int8 version = receiveBuffer.getByte();

    // only data for UDP
    int8 flags = receiveBuffer.getByte();
    if (flags < 0)
    {
        // 7-bit set
        receiveBuffer.setEndianess(EPICS_ENDIAN_BIG);
    }
    else
    {
        receiveBuffer.setEndianess(EPICS_ENDIAN_LITTLE);
    }

    // command ID and paylaod
    int8 command = receiveBuffer.getByte();
    if (command != (int8)0x04)
        return false;

    size_t payloadSize = receiveBuffer.getInt();
    if (payloadSize < (12+4+16+2))
        return false;


    epics::pvAccess::ServerGUID guid;
    receiveBuffer.get(guid.value, 0, sizeof(guid.value));

    /*int32 searchSequenceId = */receiveBuffer.getInt();

    osiSockAddr serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.ia.sin_family = AF_INET;

    // 128-bit IPv6 address
    if (!decodeAsIPv6Address(&receiveBuffer, &serverAddress))
        return false;

    // accept given address if explicitly specified by sender
    if (serverAddress.ia.sin_addr.s_addr == INADDR_ANY)
        serverAddress.ia.sin_addr = responseFrom.ia.sin_addr;

    // NOTE: htons might be a macro (e.g. vxWorks)
    int16 port = receiveBuffer.getShort();
    serverAddress.ia.sin_port = htons(port);

    string protocol = /*SerializeHelper::*/deserializeString(&receiveBuffer);

    /*bool found =*/ receiveBuffer.getByte(); // != 0;


    string guidString = toHex((int8*)guid.value, sizeof(guid.value));

    ServerMap::iterator iter = serverMap.find(guidString);
    if (iter != serverMap.end())
    {
        bool found = false;
        vector<osiSockAddr>& vec = iter->second.addresses;
        for (vector<osiSockAddr>::const_iterator ai = vec.begin();
                ai != vec.end();
                ai++)
            if (sockAddrAreIdentical(&(*ai), &serverAddress))
            {
                found = true;
                break;
            }

        if (!found)
        {
            vec.push_back(serverAddress);
            return true;
        }
        else
            return false;
    }
    else
    {
        ServerEntry serverEntry;
        serverEntry.guid = guidString;
        serverEntry.protocol = protocol;
        serverEntry.addresses.push_back(serverAddress);
        serverEntry.version = version;

        serverMap[guidString] = serverEntry;

        return true;
    }
}

bool discoverServers(double timeOut)
{
    osiSockAttach();

    SOCKET socket = epicsSocketCreate(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == INVALID_SOCKET)
    {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        fprintf(stderr, "Failed to create a socket: %s\n", errStr);
        return false;
    }

    //
    // read config
    //

    Configuration::shared_pointer configuration(new SystemConfigurationImpl());

    string addressList = configuration->getPropertyAsString("EPICS_PVA_ADDR_LIST", "");
    bool autoAddressList = configuration->getPropertyAsBoolean("EPICS_PVA_AUTO_ADDR_LIST", true);
    int broadcastPort = configuration->getPropertyAsInteger("EPICS_PVA_BROADCAST_PORT", PVA_BROADCAST_PORT);

    // quary broadcast addresses of all IFs
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
    if (!addressList.empty())
    {
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

    for (size_t i = 0; i < broadcastAddresses.size(); i++)
        LOG(logLevelDebug,
            "Broadcast address #%zu: %s.", i, inetAddressToString(broadcastAddresses[i]).c_str());

    // ---

    int optval = 1;
    int status = ::setsockopt(socket, SOL_SOCKET, SO_BROADCAST, (char *)&optval, sizeof(optval));
    if (status)
    {
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

    status = ::bind(socket, (sockaddr*)&(bindAddr.sa), sizeof(sockaddr));
    if (status)
    {
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
                           (char*)&timeout, sizeof(timeout));
    if (status)
    {
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
    sendBuffer.putByte(PVA_VERSION);
    sendBuffer.putByte((EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG) ? 0x80 : 0x00); // data + 7-bit endianess
    sendBuffer.putByte((int8_t)CMD_SEARCH);	// search
    sendBuffer.putInt(4+1+3+16+2+1+2);		// "zero" payload

    sendBuffer.putInt(0);   // sequenceId
    sendBuffer.putByte((int8_t)0x81);    // reply required // TODO unicast vs multicast; for now we mark ourselves as unicast
    sendBuffer.putByte((int8_t)0);    // reserved
    sendBuffer.putShort((int16_t)0);  // reserved

    // NOTE: is it possible (very likely) that address is any local address ::ffff:0.0.0.0
    encodeAsIPv6Address(&sendBuffer, &responseAddress);
    sendBuffer.putShort((int16_t)ntohs(responseAddress.ia.sin_port));

    sendBuffer.putByte((int8_t)0x00);	// protocol count
    sendBuffer.putShort((int16_t)0);	// name count

    bool oneOK = false;
    for (size_t i = 0; i < broadcastAddresses.size(); i++)
    {
        if(pvAccessIsLoggable(logLevelDebug)) {
            char strBuffer[64];
            sockAddrToDottedIP(&broadcastAddresses[i].sa, strBuffer, sizeof(strBuffer));
            LOG(logLevelDebug, "UDP Tx (%zu) -> %s", sendBuffer.getPosition(), strBuffer);
        }

        status = ::sendto(socket, sendBuffer.getArray(), sendBuffer.getPosition(), 0,
                          &broadcastAddresses[i].sa, sizeof(sockaddr));
        if (status < 0)
        {
            char errStr[64];
            epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
            fprintf(stderr, "Send error: %s\n", errStr);
        }
        else
            oneOK = true;
    }

    if (!oneOK)
        return false;


    char rxbuff[1024];
    ByteBuffer receiveBuffer(rxbuff, sizeof(rxbuff)/sizeof(char));

    osiSockAddr fromAddress;
    osiSocklen_t addrStructSize = sizeof(sockaddr);

    int sendCount = 0;

    while (true)
    {
        receiveBuffer.clear();

        // receive packet from socket
        int bytesRead = ::recvfrom(socket, (char*)receiveBuffer.getArray(),
                                   receiveBuffer.getRemaining(), 0,
                                   (sockaddr*)&fromAddress, &addrStructSize);

        if (bytesRead > 0)
        {
            if(pvAccessIsLoggable(logLevelDebug)) {
                char strBuffer[64];
                sockAddrToDottedIP(&fromAddress.sa, strBuffer, sizeof(strBuffer));
                LOG(logLevelDebug, "UDP Rx (%d) <- %s", bytesRead, strBuffer);
            }
            receiveBuffer.setPosition(bytesRead);
            receiveBuffer.flip();

            processSearchResponse(fromAddress, receiveBuffer);

        }
        else
        {
            if (bytesRead == -1)
            {
                int socketError = SOCKERRNO;

                // interrupted or timeout
                if (socketError == SOCK_EINTR ||
                        socketError == EAGAIN ||        // no alias in libCom
                        // windows times out with this
                        socketError == SOCK_ETIMEDOUT ||
                        socketError == SOCK_EWOULDBLOCK)
                {
                    // OK
                }
                else if (socketError == SOCK_ECONNREFUSED || // avoid spurious ECONNREFUSED in Linux
                         socketError == SOCK_ECONNRESET)     // or ECONNRESET in Windows
                {
                    // OK
                }
                else
                {
                    // unexpected error
                    char errStr[64];
                    epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                    fprintf(stderr, "Socket recv error: %s\n", errStr);
                    break;
                }

            }

            if (++sendCount < 3)
            {
                // TODO duplicate code
                bool oneOK = false;
                for (size_t i = 0; i < broadcastAddresses.size(); i++)
                {
                    // send the packet
                    status = ::sendto(socket, sendBuffer.getArray(), sendBuffer.getPosition(), 0,
                                      &broadcastAddresses[i].sa, sizeof(sockaddr));
                    if (status < 0)
                    {
                        char errStr[64];
                        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                        fprintf(stderr, "Send error: %s\n", errStr);
                    }
                    else
                        oneOK = true;
                }

                if (!oneOK)
                    return false;

            }
            else
                break;
        }

    }

    // TODO shutdown sockets?
    // TODO this resouce is not released on failure
    epicsSocketDestroy(socket);

    return true;
}


#define DEFAULT_TIMEOUT 3.0

void usage (void)
{
    fprintf (stderr, "\nUsage: pvlist [options] [<server address or GUID starting with '0x'>]...\n\n"
             "\noptions:\n"
             "  -h: Help: Print this message\n"
             "  -V: Print version and exit\n"
             "  -i                 Print server info (when server address list/GUID is given)\n"
             "  -w <sec>:          Wait time, specifies timeout, default is %f second(s)\n"
             "  -q:                Quiet mode, print only error messages\n"
             "  -d:                Enable debug output\n"
//    "  -F <ofs>:          Use <ofs> as an alternate output field separator\n"
//    "  -f <input file>:   Use <input file> as an input that provides a list input parameters(s) to be read, use '-' for stdin\n"
             "\nexamples:\n"
             "\tpvlist\n"
             "\tpvlist ioc0001\n"
             "\tpvlist 10.5.1.205:10000\n"
             "\tpvlist 0x83DE3C540000000000BF351F\n\n"
             , DEFAULT_TIMEOUT);
}

}//namespace

/*+**************************************************************************
 *
 * Function:	main
 *
 * Description:	pvlist main()
 * 		Evaluate command line options, ...
 *
 * Arg(s) In:	[options] [<server>]...
 *
 * Arg(s) Out:	none
 *
 * Return(s):	Standard return code (0=success, 1=error)
 *
 **************************************************************************-*/

int main (int argc, char *argv[])
{
    int opt;                    /* getopt() current option */
    bool debug = false;
    double timeOut = DEFAULT_TIMEOUT;
    bool printInfo = false;

    /*
    istream* inputStream = 0;
    ifstream ifs;
    bool fromStream = false;
    */
    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */

    while ((opt = getopt(argc, argv, ":hVw:qdF:f:i")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'V':               /* Print version */
        {
            Version version("pvlist", "cpp",
                    EPICS_PVA_MAJOR_VERSION,
                    EPICS_PVA_MINOR_VERSION,
                    EPICS_PVA_MAINTENANCE_VERSION,
                    EPICS_PVA_DEVELOPMENT_FLAG);
            fprintf(stdout, "%s\n", version.getVersionString().c_str());
            return 0;
        }
        case 'w':               /* Set PVA timeout value */
            if((epicsScanDouble(optarg, &timeOut)) != 1 || timeOut <= 0.0)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('pvlist -h' for help.)\n", optarg);
                timeOut = DEFAULT_TIMEOUT;
            }
            break;
        case 'q':               /* Quiet mode */
            break;
        case 'd':               /* Debug log level */
            debug = true;
            break;
        case 'i':               /* Print server info */
            printInfo = true;
            break;
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('pvlist -h' for help.)\n",
                    optopt);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('pvlist -h' for help.)\n",
                    optopt);
            return 1;
        default :
            usage();
            return 1;
        }
    }

    SET_LOG_LEVEL(debug ? logLevelDebug : logLevelError);

    bool noArgs = (optind == argc);

    bool byGUIDSearch = false;
    for (int i = optind; i < argc; i++)
    {
        string serverAddress = argv[i];

        // by GUID search
        if (serverAddress.length() == 26 &&
                serverAddress[0] == '0' &&
                serverAddress[1] == 'x')
        {
            byGUIDSearch = true;
            break;
        }
    }

    bool allOK = true;

    if (noArgs || byGUIDSearch)
        discoverServers(timeOut);

    // just list all the discovered servers
    if (noArgs)
    {
        for (ServerMap::const_iterator iter = serverMap.begin();
                iter != serverMap.end();
                iter++)
        {
            const ServerEntry& entry = iter->second;

            cout << "GUID 0x" << entry.guid << " version " << (int)entry.version << ": "
                 << entry.protocol << "@[";

            size_t count = entry.addresses.size();
            for (size_t i = 0; i < count; i++)
            {
                cout << inetAddressToString(entry.addresses[i]);
                if (i < (count-1))
                    cout << ", ";
            }
            cout << ']' << endl;
        }
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            string serverAddress = argv[i];

            // by GUID search
            if (serverAddress.length() == 26 &&
                    serverAddress[0] == '0' &&
                    serverAddress[1] == 'x')
            {
                bool resolved = false;
                for (ServerMap::const_iterator iter = serverMap.begin();
                        iter != serverMap.end();
                        iter++)
                {
                    const ServerEntry& entry = iter->second;

                    if (strncmp(entry.guid.c_str(), &(serverAddress[2]), 24) == 0)
                    {
                        // found match

                        // TODO for now we take only first server address
                        serverAddress = inetAddressToString(entry.addresses[0]);
                        resolved = true;
                        break;
                    }
                }

                if (!resolved)
                {
                    fprintf(stderr, "Failed to resolve GUID '%s'!\n", serverAddress.c_str());
                    allOK = false;
                    continue;
                }
            }

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
            args->getSubFieldT<PVString>("query.op")->put(printInfo ? "info" : "channels");

            if(debug) {
                std::cerr<<"Query to "<<serverAddress<<"\n"<<args<<"\n";
            }

            PVStructure::shared_pointer ret;
            try {
                RPCClient rpc("server",
                              createRequest("field()"),
                              ChannelProvider::shared_pointer(),
                              serverAddress);

                if(debug)
                    std::cerr<<"Execute\n";
                ret = rpc.request(args, timeOut, true);
            } catch(std::exception& e) {
                std::cerr<<"Error: "<<e.what()<<"\n";
                return 1;
            }

            if(!printInfo) {
                PVStringArray::shared_pointer pvs(ret->getSubField<PVStringArray>("value"));

                PVStringArray::const_svector val(pvs->view());

                std::copy(val.begin(),
                          val.end(),
                          std::ostream_iterator<std::string>(std::cout, "\n"));

                return allOK ? 0 : 1;
            }

            std::cout<<ret<<"\n";
        }
    }

    return allOK ? 0 : 1;
}
