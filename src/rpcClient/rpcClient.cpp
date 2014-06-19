/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvAccessCPP is distributed subject to a Software License Agreement
 * found in file LICENSE that is included with this distribution.
 */

#include <iostream>
#include <string>

#include <pv/pvData.h>
#include <pv/convert.h>
#include <pv/event.h>

#define epicsExportSharedSymbols
#include <pv/pvAccess.h>
#include <pv/clientFactory.h>
#include <pv/logger.h>
#include <pv/rpcService.h>

#include "rpcClient.h"


using namespace epics::pvData;
using std::string;

namespace epics
{

namespace pvAccess
{

namespace 
{

// copied from eget/pvutils. This is a lot of boilerplate and needs refactoring to common code.

class ChannelRequesterImpl :
    public epics::pvAccess::ChannelRequester
{
    private:
        epics::pvData::Event m_event;
    
    public:
        virtual std::string getRequesterName();
        virtual void message(std::string const & message, epics::pvData::MessageType messageType);
    
        virtual void channelCreated(const epics::pvData::Status& status, epics::pvAccess::Channel::shared_pointer const & channel);
        virtual void channelStateChange(epics::pvAccess::Channel::shared_pointer const & channel, epics::pvAccess::Channel::ConnectionState connectionState);
    
        bool waitUntilConnected(double timeOut);
};

class ChannelRPCRequesterImpl : public ChannelRPCRequester
{
    private:
    ChannelRPC::shared_pointer m_channelRPC;
    Mutex m_pointerMutex;
    Event m_event;
    Event m_connectionEvent;
    string m_channelName;

    public:
    epics::pvData::PVStructure::shared_pointer response;   
    ChannelRPCRequesterImpl(std::string channelName) : m_channelName(channelName) {}
    
    virtual string getRequesterName()
    {
        return "ChannelRPCRequesterImpl";
    }

    virtual void message(std::string const & message, MessageType messageType)
    {
        std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelRPCConnect(const epics::pvData::Status& status, ChannelRPC::shared_pointer const & channelRPC)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel RPC create: " << status << std::endl;
            }
            
            // assign smart pointers
            {
                Lock lock(m_pointerMutex);
                m_channelRPC = channelRPC;
            }
            
            m_connectionEvent.signal();
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to create channel get: " << status << std::endl;
        }
    }

    virtual void requestDone(const epics::pvData::Status &status, ChannelRPC::shared_pointer const & /*channelRPC*/,
            epics::pvData::PVStructure::shared_pointer const &pvResponse)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel RPC: " << status << std::endl;
            }

            // access smart pointers
            {
                Lock lock(m_pointerMutex);

                response = pvResponse;
                // this is OK since calle holds reference to it
                m_channelRPC.reset();                
            }
            
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to RPC: " << status << std::endl;
            {
                Lock lock(m_pointerMutex);
                // this is OK since calle holds reference to it
                m_channelRPC.reset();
            }
        }
        
        m_event.signal();
    }
    
    /*
    void request(epics::pvData::PVStructure::shared_pointer const &pvRequest)
    {
        Lock lock(m_pointerMutex);
        m_channelRPC->request(pvRequest, false);
    }
    */

    bool waitUntilRPC(double timeOut)
    {
        return m_event.wait(timeOut);
    }

    bool waitUntilConnected(double timeOut)
    {
        return m_connectionEvent.wait(timeOut);
    }
};


string ChannelRequesterImpl::getRequesterName()
{
	return "ChannelRequesterImpl";
}

void ChannelRequesterImpl::message(std::string const & message, MessageType messageType)
{
	std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
}

void ChannelRequesterImpl::channelCreated(const epics::pvData::Status& status, Channel::shared_pointer const & channel)
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
	}
}

void ChannelRequesterImpl::channelStateChange(Channel::shared_pointer const & /*channel*/, Channel::ConnectionState connectionState)
{
	if (connectionState == Channel::CONNECTED)
	{
		m_event.signal();
	}
	/*
	else if (connectionState != Channel::DESTROYED)
	{
		std::cerr << "[" << channel->getChannelName() << "] channel state change: "  << Channel::ConnectionStateNames[connectionState] << std::endl;
	}
	*/
}
    
bool ChannelRequesterImpl::waitUntilConnected(double timeOut)
{
	return m_event.wait(timeOut);
}


class RPCClientImpl: public RPCClient
{

public:
    POINTER_DEFINITIONS(RPCClientImpl);

    RPCClientImpl(const std::string & serviceName)
        : m_serviceName(serviceName), m_connected(false)
    {
    }

    virtual PVStructure::shared_pointer request(PVStructure::shared_pointer pvRequest, double timeOut);

private:
    void init()
    {
        using namespace std::tr1;
        m_provider = getChannelProviderRegistry()->getProvider("pva");
    
        shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl()); 
        m_channelRequesterImpl = channelRequesterImpl;
        m_channel = m_provider->createChannel(m_serviceName, channelRequesterImpl);
    }

    bool connect(double timeOut)
    {
        init();
        m_connected = m_channelRequesterImpl->waitUntilConnected(timeOut);             
        return m_connected;      
    }

    std::string m_serviceName;
    ChannelProvider::shared_pointer m_provider;
    std::tr1::shared_ptr<ChannelRequesterImpl> m_channelRequesterImpl;
    Channel::shared_pointer m_channel;
    bool m_connected;
};


PVStructure::shared_pointer RPCClientImpl::request(PVStructure::shared_pointer pvRequest, double timeOut)
{
    using namespace std::tr1;

    PVStructure::shared_pointer response;

    bool allOK = true;

    if (m_connected || connect(timeOut))
    {
        shared_ptr<ChannelRPCRequesterImpl> rpcRequesterImpl(new ChannelRPCRequesterImpl(m_channel->getChannelName()));
        ChannelRPC::shared_pointer channelRPC = m_channel->createChannelRPC(rpcRequesterImpl, pvRequest);

        if (rpcRequesterImpl->waitUntilConnected(timeOut))
        {
            channelRPC->lastRequest();
            channelRPC->request(pvRequest);
            allOK &= rpcRequesterImpl->waitUntilRPC(timeOut);
            response = rpcRequesterImpl->response;
        }
        else
        {
            allOK = false;
            m_channel->destroy();
            m_connected =  false;
            std::string errMsg = "[" + m_channel->getChannelName() + "] RPC create timeout";
            throw epics::pvAccess::RPCRequestException(Status::STATUSTYPE_ERROR, errMsg);
        }
    }
    else
    {
        allOK = false;
        m_channel->destroy();
        m_connected = false;
        std::string errMsg = "[" + m_channel->getChannelName() + "] connection timeout";
        throw epics::pvAccess::RPCRequestException(Status::STATUSTYPE_ERROR, errMsg);
    }

    if (!allOK)
    {
        throw epics::pvAccess::RPCRequestException(Status::STATUSTYPE_ERROR, "RPC request failed");
    }

    return response;
}

}

RPCClient::shared_pointer RPCClientFactory::create(const std::string & serviceName)
{
    ClientFactory::start();
    
    return RPCClient::shared_pointer(new RPCClientImpl(serviceName));
}


PVStructure::shared_pointer sendRequest(const std::string & serviceName,
     PVStructure::shared_pointer queryRequest,
     double timeOut)
{
    RPCClient::shared_pointer client = RPCClientFactory::create(serviceName);

    return client->request(queryRequest, timeOut);
}


}

}

