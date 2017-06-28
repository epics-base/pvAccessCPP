#include <pv/clientFactory.h>
#include <pv/pvAccess.h>
#include <iostream>
#include <epicsThread.h>

namespace pva = epics::pvAccess;

class ChannelGetRequesterImpl : public pva::ChannelGetRequester
{
public:
    virtual ~ChannelGetRequesterImpl() {
        std::cout << "~ChannelGetRequesterImpl" << std::endl;
    }

    virtual std::string getRequesterName() {
        return "ChannelGetRequesterImpl";
    }

    virtual void message(std::string const & message, epics::pvData::MessageType messageType) {
        std::cout << message << std::endl;
    }

    virtual void channelGetConnect(
        const epics::pvData::Status& status,
        pva::ChannelGet::shared_pointer const & channelGet,
        epics::pvData::Structure::const_shared_pointer const & structure)
    {
        if (status.isSuccess())
        {
            std::cout << "issuing channel get" << std::endl;
            channelGet->get();
        }
        else
            std::cout << "failed to create channel get: " << status << std::endl;
    }

    virtual void getDone(
        const epics::pvData::Status& status,
        pva::ChannelGet::shared_pointer const & channelGet,
        epics::pvData::PVStructure::shared_pointer const & pvStructure,
        epics::pvData::BitSet::shared_pointer const & bitSet)
    {
        if (status.isSuccess())
        {
            std::cout << *pvStructure << std::endl;
        }
        else
            std::cout << "failed to get: " << status << std::endl;

    }
};

class ChannelRequesterImpl : public pva::ChannelRequester
{
public:
    virtual ~ChannelRequesterImpl() {
        std::cout << "~ChannelRequesterImpl" << std::endl;
    }

    virtual std::string getRequesterName() {
        return "ChannelRequesterImpl";
    }

    virtual void message(std::string const & message, epics::pvData::MessageType messageType) {
        std::cout << message << std::endl;
    }

    virtual void channelCreated(const epics::pvData::Status& status,
                                pva::Channel::shared_pointer const & channel)
    {
        if (status.isSuccess())
        {

        }
        else
            std::cout << "failed to create channel: " << status << std::endl;
    }

    /**
     * A channel connection state change has occurred.
     * @param c The channel.
     * @param connectionState The new connection state.
     */
    virtual void channelStateChange(pva::Channel::shared_pointer const & channel, pva::Channel::ConnectionState connectionState)
    {
        std::cerr << "[" << channel->getChannelName() << "] channel state change: "
                  << pva::Channel::ConnectionStateNames[connectionState] << std::endl;
    }
};

int main()
{
    pva::ClientFactory::start();

    pva::ChannelProvider::shared_pointer provider =
            pva::ChannelProviderRegistry::clients()->getProvider("pva");


    {
        pva::ChannelRequester::shared_pointer channelRequester(new ChannelRequesterImpl());
        pva::Channel::shared_pointer channel = provider->createChannel("testChannel", channelRequester);

        {
            pva::ChannelGetRequester::shared_pointer channelGetRequester(new ChannelGetRequesterImpl());
            pva::ChannelGet::shared_pointer channelGet =
                channel->createChannelGet(channelGetRequester,
                                          epics::pvData::CreateRequest::create()->createRequest(""));

            epicsThreadSleep(3.0);
            std::cout << "leaving channelGet scope, channelGet should be destroyed" << std::endl;
        }

        epicsThreadSleep(3.0);
        std::cout << "leaving channel scope, channel should be destroyed" << std::endl;
    }

    epicsThreadSleep(3.0);
    std::cout << "exiting" << std::endl;
    return 0;
}
