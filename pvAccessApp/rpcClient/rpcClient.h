/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvAccessCPP is distributed subject to a Software License Agreement
 * found in file LICENSE that is included with this distribution.
 */

#ifndef RPCCLIENT_H
#define RPCCLIENT_H

#include <string>

#include <pv/pvData.h>


namespace epics
{

namespace pvAccess
{
    /**
     * RPCClient is an interface class that is used by a service client.
     *
     */
    class RPCClient
    {
    public:
        POINTER_DEFINITIONS(RPCClient);


        /**
	     * Sends a request and wait for the response or until timeout occurs.
	     * This method will also wait for client to connect, if necessary.
         *
	     * @param  pvArgument  the argument for the rpc
	     * @param  timeout     the time in seconds to wait for the response
	     * @return             request response.
	     */
        virtual epics::pvData::PVStructure::shared_pointer request(epics::pvData::PVStructure::shared_pointer pvRequest,
            double timeOut) = 0;

        virtual ~RPCClient() {}
    };



    class RPCClientFactory
    {
    public:
	    /**
	     * Create a RPCClient.
         *
	     * @param  serviceName  the service name 
	     * @return              the RPCClient interface
	     */
	    static RPCClient::shared_pointer create(const std::string & serviceName);
    };

    /**
     * Performs complete blocking RPC call, opening a channel and connecting to the
     * service and sending the request.
     * The PVStructure sent on connection is null.
     *
     * @param  serviceName         the name of the service to connect to
     * @param  request             the request sent to the service
     * @return                     the result of the RPC call.
     */
    epics::pvData::PVStructure::shared_pointer sendRequest(const std::string & serviceName,
        epics::pvData::PVStructure::shared_pointer request, double timeOut);
}

}

#endif


