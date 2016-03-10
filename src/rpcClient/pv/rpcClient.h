/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvAccessCPP is distributed subject to a Software License Agreement
 * found in file LICENSE that is included with this distribution.
 */

#ifndef RPCCLIENT_H
#define RPCCLIENT_H

#include <string>

#ifdef epicsExportSharedSymbols
#   define rpcClientEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif
#include <pv/pvData.h>
#ifdef rpcClientEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef rpcClientEpicsExportSharedSymbols
#endif

#include <pv/rpcService.h>

#include <shareLib.h>

#define RPCCLIENT_DEFAULT_TIMEOUT 5.0

namespace epics
{

namespace pvAccess
{

/**
 * RPCClient is an interface class that is used by a service client.
 *
 */
class epicsShareClass RPCClient
{
public:
    POINTER_DEFINITIONS(RPCClient);

    /**
     * Create a RPCClient.
     *
     * @param  serviceName  the service name
     * @return              the RPCClient interface
     */
    static shared_pointer create(const std::string & serviceName);

    /**
     * Create a RPCClient.
     *
     * @param  serviceName  the service name
     * @param  pvRequest    the pvRequest for the ChannelRPC
     * @return              the RPCClient interface
     */
    static shared_pointer create(const std::string & serviceName,
                                 epics::pvData::PVStructure::shared_pointer const & pvRequest);

    /**
     * Performs complete blocking RPC call, opening a channel and connecting to the
     * service and sending the request.
     *
     * @param serviceName         the name of the service to connect to
     * @param request             the request sent to the service
     * @param timeout             the timeout (in seconds), 0 means forever.
     * @return                    the result of the RPC call.
     * @throws RPCRequestException exception thrown on error on timeout.
     */
    static epics::pvData::PVStructure::shared_pointer sendRequest(const std::string & serviceName,
            epics::pvData::PVStructure::shared_pointer const &request, double timeOut = RPCCLIENT_DEFAULT_TIMEOUT);



    /**
     * Destroy this instance (i.e. release resources).
     */
    void destroy();

    /**
     * Connect to the server.
     * The method blocks until the connection is made or a timeout occurs.
     * It is the same as calling issueConnect and then waitConnect.
     * @param timeout Timeout in seconds to wait, 0 means forever.
     * @returns (false,true) If (not connected, is connected).
     * If false then connect must be reissued.
     */
    bool connect(double timeout = RPCCLIENT_DEFAULT_TIMEOUT);

    /**
     * Issue a connect request and return immediately.
     * waitConnect must be called to complete the request.
     */
    void issueConnect();

    /**
     * Wait for the connect request to complete.
     * @param timeout timeout in seconds to wait, 0 means forever.
     * @returns (false,true) If (not connected, is connected).
     * If false then connect must be reissued.
     */
    bool waitConnect(double timeout = RPCCLIENT_DEFAULT_TIMEOUT);

    /**
     * Sends a request and wait for the response or until timeout occurs.
     * This method will also wait for client to connect, if necessary.
     *
     * @param  pvArgument  the argument for the rpc
     * @param  timeout     the time in seconds to wait for the response, 0 means forever.
     * @param  lastRequest If true an automatic destroy is made.
     * @return             request response.
     * @throws RPCRequestException exception thrown on error or timeout.
     */
    epics::pvData::PVStructure::shared_pointer request(
        epics::pvData::PVStructure::shared_pointer const & pvArgument,
        double timeout = RPCCLIENT_DEFAULT_TIMEOUT,
        bool lastRequest = false);

    /**
     * Issue a channelRPC request and return immediately.
     * waitRequest must be called to complete the request.
     * @param pvAgument The argument to pass to the server.
     * @param lastRequest If true an automatic destroy is made.
     * @throws std::runtime_error excption thrown on any runtime error condition (e.g. no connection made).
     */
    void issueRequest(
        epics::pvData::PVStructure::shared_pointer const & pvArgument,
        bool lastRequest = false);

    /**
     * Wait for the request to complete.
     * @param timeout      the time in seconds to wait for the reponse.
     * @return             request response.
     * @throws RPCRequestException exception thrown on error or timeout.
     */
    epics::pvData::PVStructure::shared_pointer waitResponse(double timeout = RPCCLIENT_DEFAULT_TIMEOUT);

    virtual ~RPCClient() {}

protected:
    RPCClient(const std::string & serviceName,
              epics::pvData::PVStructure::shared_pointer const & pvRequest);

    std::string m_serviceName;
    Channel::shared_pointer m_channel;
    epics::pvData::PVStructure::shared_pointer m_pvRequest;
};

}

}

#endif


