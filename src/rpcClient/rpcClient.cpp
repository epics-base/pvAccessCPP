/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvAccessCPP is distributed subject to a Software License Agreement
 * found in file LICENSE that is included with this distribution.
 */

#include <iostream>
#include <string>

#include <pv/pvData.h>
#include <pv/event.h>

#define epicsExportSharedSymbols
#include <pv/pvAccess.h>
#include <pv/clientFactory.h>
#include <pv/logger.h>
#include <pv/rpcService.h>

#include "pv/rpcClient.h"


using namespace epics::pvData;
namespace TR1 = std::tr1;
using std::string;

namespace epics
{

namespace pvAccess
{



class ChannelAndRPCRequesterImpl :
    public TR1::enable_shared_from_this<ChannelAndRPCRequesterImpl>,
    public virtual epics::pvAccess::ChannelRequester,
    public virtual epics::pvAccess::ChannelRPCRequester
{
private:
    Mutex m_mutex;
    Event m_event;
    Event m_connectionEvent;

    Status m_status;
    PVStructure::shared_pointer m_response;
    ChannelRPC::shared_pointer m_channelRPC;
    PVStructure::shared_pointer m_pvRequest;

public:

    ChannelAndRPCRequesterImpl(PVStructure::shared_pointer const & pvRequest)
        : m_pvRequest(pvRequest)
    {
    }

    virtual string getRequesterName()
    {
        return "ChannelAndRPCRequesterImpl";
    }

    virtual void message(std::string const & message, MessageType messageType)
    {
        std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    void channelCreated(
        const epics::pvData::Status& status,
        Channel::shared_pointer const & channel)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << channel->getChannelName() << "] channel create: " << status << std::endl;
            }
        }
        else
        {
            std::cerr << "[" << channel->getChannelName() << "] failed to create a channel: " << status << std::endl;

            {
                Lock lock(m_mutex);
                m_status = status;
            }
            m_connectionEvent.signal();
        }
    }

    void channelStateChange(
        Channel::shared_pointer const & channel,
        Channel::ConnectionState connectionState)
    {
        if (connectionState == Channel::CONNECTED)
        {
            bool rpcAlreadyConnectedOnce = false;
            {
                Lock lock(m_mutex);
                rpcAlreadyConnectedOnce = (m_channelRPC.get() != 0);
            }

            if (!rpcAlreadyConnectedOnce)
            {
                channel->createChannelRPC(shared_from_this(), m_pvRequest);
            }
        }
        /*
        else if (connectionState != Channel::DESTROYED)
        {
            std::cerr << "[" << channel->getChannelName() << "] channel state change: "  << Channel::ConnectionStateNames[connectionState] << std::endl;
        }
        */
    }

    virtual void channelRPCConnect(
        const epics::pvData::Status & status,
        ChannelRPC::shared_pointer const & channelRPC)
    {
        if (status.isSuccess())
        {
            if (!status.isOK())
                std::cerr << "[" << channelRPC->getChannel()->getChannelName() << "] channel RPC create: " << status << std::endl;
        }
        else
        {
            std::cerr << "[" << channelRPC->getChannel()->getChannelName() << "] failed to create channel RPC: " << status << std::endl;
        }

        {
            Lock lock(m_mutex);
            m_status = status;
            m_channelRPC = channelRPC;
        }

        m_connectionEvent.signal();
    }

    virtual void requestDone(
        const epics::pvData::Status & status,
        ChannelRPC::shared_pointer const & channelRPC,
        epics::pvData::PVStructure::shared_pointer const & pvResponse)
    {
        if (status.isSuccess())
        {
            if (!status.isOK())
                std::cerr << "[" << channelRPC->getChannel()->getChannelName() << "] channel RPC: " << status << std::endl;
        }
        else
        {
            std::cerr << "[" << channelRPC->getChannel()->getChannelName() << "] failed to RPC: " << status << std::endl;
        }

        {
            Lock lock(m_mutex);
            m_status = status;
            m_response = pvResponse;
        }

        m_event.signal();
    }

    bool waitForResponse(double timeOut)
    {
        return m_event.wait(timeOut);
    }

    bool waitUntilRPCConnected(double timeOut)
    {
        if (isRPCConnected())
            return true;

        return m_connectionEvent.wait(timeOut);
    }

    bool isRPCConnected()
    {
        Lock lock(m_mutex);
        return (m_channelRPC.get() != 0);
    }

    PVStructure::shared_pointer & getResponse()
    {
        Lock lock(m_mutex);
        return m_response;
    }

    Status & getStatus()
    {
        Lock lock(m_mutex);
        return m_status;
    }

    void request(PVStructure::shared_pointer const & pvArgument, bool lastRequest)
    {
        ChannelRPC::shared_pointer rpc;
        {
            Lock lock(m_mutex);
            rpc = m_channelRPC;
        }

        if (!rpc)
            throw std::runtime_error("channel RPC not connected");

        if (lastRequest)
            rpc->lastRequest();

        rpc->request(pvArgument);
    }
};












RPCClient::RPCClient(const std::string & serviceName,
                     PVStructure::shared_pointer const & pvRequest)
    : m_serviceName(serviceName), m_pvRequest(pvRequest)
{
}

void RPCClient::destroy()
{
    if (m_channel)
    {
        m_channel->destroy();
        m_channel.reset();
    }
}

bool RPCClient::connect(double timeout)
{
    if (m_channel &&
            TR1::dynamic_pointer_cast<ChannelAndRPCRequesterImpl>(m_channel->getChannelRequester())->isRPCConnected())
        return true;

    issueConnect();
    return waitConnect(timeout);
}

void RPCClient::issueConnect()
{
    ChannelProvider::shared_pointer provider = getChannelProviderRegistry()->getProvider("pva");

    // TODO try to reuse ChannelRequesterImpl instance (i.e. create only once)
    TR1::shared_ptr<ChannelAndRPCRequesterImpl> channelRequesterImpl(new ChannelAndRPCRequesterImpl(m_pvRequest));
    m_channel = provider->createChannel(m_serviceName, channelRequesterImpl);
}

bool RPCClient::waitConnect(double timeout)
{
    if (!m_channel)
        throw std::runtime_error("issueConnect() must be called before waitConnect()");

    TR1::shared_ptr<ChannelAndRPCRequesterImpl> channelRequesterImpl =
        TR1::dynamic_pointer_cast<ChannelAndRPCRequesterImpl>(m_channel->getChannelRequester());

    return channelRequesterImpl->waitUntilRPCConnected(timeout) &&
           channelRequesterImpl->isRPCConnected();
}



PVStructure::shared_pointer RPCClient::request(
    PVStructure::shared_pointer const & pvArgument,
    double timeout,
    bool lastRequest)
{
    if (connect(timeout))
    {
        issueRequest(pvArgument, lastRequest);
        return waitResponse(timeout);       // TODO reduce timeout for a time spent on connect
    }
    else
        throw epics::pvAccess::RPCRequestException(Status::STATUSTYPE_ERROR, "connection timeout");
}

void RPCClient::issueRequest(
    PVStructure::shared_pointer const & pvArgument,
    bool lastRequest)
{
    if (!m_channel)
        throw std::runtime_error("channel not connected");

    TR1::shared_ptr<ChannelAndRPCRequesterImpl> channelRequesterImpl =
        TR1::dynamic_pointer_cast<ChannelAndRPCRequesterImpl>(m_channel->getChannelRequester());

    channelRequesterImpl->request(pvArgument, lastRequest);
}

PVStructure::shared_pointer RPCClient::waitResponse(double timeout)
{
    TR1::shared_ptr<ChannelAndRPCRequesterImpl> channelRequesterImpl =
        TR1::dynamic_pointer_cast<ChannelAndRPCRequesterImpl>(m_channel->getChannelRequester());

    if (channelRequesterImpl->waitForResponse(timeout))
    {
        Status & status = channelRequesterImpl->getStatus();
        if (status.isSuccess())
        {
            // release response structure
            PVStructure::shared_pointer & response = channelRequesterImpl->getResponse();
            PVStructure::shared_pointer retVal = response;
            response.reset();
            return retVal;
        }
        else
            throw epics::pvAccess::RPCRequestException(status.getType(), status.getMessage());
    }
    else
        throw epics::pvAccess::RPCRequestException(Status::STATUSTYPE_ERROR, "RPC timeout");
}

RPCClient::shared_pointer RPCClient::create(const std::string & serviceName)
{
    PVStructure::shared_pointer pvRequest =
        CreateRequest::create()->createRequest("");
    return create(serviceName, pvRequest);
}

RPCClient::shared_pointer RPCClient::create(const std::string & serviceName,
        PVStructure::shared_pointer const & pvRequest)
{
    ClientFactory::start();
    return RPCClient::shared_pointer(new RPCClient(serviceName, pvRequest));
}

PVStructure::shared_pointer RPCClient::sendRequest(const std::string & serviceName,
        PVStructure::shared_pointer const & queryRequest,
        double timeOut)
{
    RPCClient::shared_pointer client = RPCClient::create(serviceName);
    return client->request(queryRequest, timeOut);
}


}

}

