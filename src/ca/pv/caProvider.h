/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef CAPROVIDER_H
#define CAPROVIDER_H

#include <cadef.h>

#include <pv/pvAccess.h>
#include <map>

#include <shareLib.h>

namespace epics {
namespace pvAccess {
class Configuration;
namespace ca {

class epicsShareClass CAChannelProvider :
    public epics::pvAccess::ChannelProvider,
    public std::tr1::enable_shared_from_this<CAChannelProvider>
{
public:
    POINTER_DEFINITIONS(CAChannelProvider);

    CAChannelProvider();
    CAChannelProvider(const std::tr1::shared_ptr<Configuration>&);
    virtual ~CAChannelProvider();

    /* --------------- epics::pvAccess::ChannelProvider --------------- */

    virtual std::string getProviderName();

    virtual ChannelFind::shared_pointer channelFind(
        std::string const & channelName,
        ChannelFindRequester::shared_pointer const & channelFindRequester);

    virtual ChannelFind::shared_pointer channelList(
        ChannelListRequester::shared_pointer const & channelListRequester);

    virtual Channel::shared_pointer createChannel(
        std::string const & channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        short priority);

    virtual Channel::shared_pointer createChannel(
        std::string const & channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        short priority,
        std::string const & address);

    virtual void configure(epics::pvData::PVStructure::shared_pointer configuration);
    virtual void flush();
    virtual void poll();

    virtual void destroy();

    /* ---------------------------------------------------------------- */

    void threadAttach();

    void registerChannel(Channel::shared_pointer const & channel);
    void unregisterChannel(Channel::shared_pointer const & channel);
    void unregisterChannel(Channel* pchannel);

private:

    void initialize();

    ca_client_context* current_context;

    epics::pvData::Mutex channelsMutex;
    // TODO std::unordered_map
    // void* is not the nicest thing, but there is no fast weak_ptr::operator==
    typedef std::map<void*, Channel::weak_pointer> ChannelList;
    ChannelList channels;

    // synced on channelsMutex
    bool destroyed;
};


class epicsShareClass CAClientFactory {
private:
    static epics::pvAccess::ChannelProviderRegistry::shared_pointer channelRegistry;
    static epics::pvAccess::ChannelProviderFactory::shared_pointer channelProvider;
    static int numStart;
public:
    static void start();
    static void stop();
};

}
}
}

#endif  /* CAPROVIDER_H */
