/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef CAPROVIDER_H
#define CAPROVIDER_H

#include <pv/pvAccess.h>
#include <map>

namespace epics {
namespace pvAccess {
namespace ca {

class CAChannelProvider :
        public ChannelProvider,
        public std::tr1::enable_shared_from_this<CAChannelProvider>
{
public:

    static epics::pvData::String PROVIDER_NAME;

    CAChannelProvider();
    virtual ~CAChannelProvider();

    /* --------------- epics::pvAccess::ChannelProvider --------------- */

    virtual epics::pvData::String getProviderName();

    virtual ChannelFind::shared_pointer channelFind(
            epics::pvData::String const & channelName,
            ChannelFindRequester::shared_pointer const & channelFindRequester);


    virtual Channel::shared_pointer createChannel(
            epics::pvData::String const & channelName,
            ChannelRequester::shared_pointer const & channelRequester,
            short priority);

    virtual Channel::shared_pointer createChannel(
            epics::pvData::String const & channelName,
            ChannelRequester::shared_pointer const & channelRequester,
            short priority,
            epics::pvData::String const & address);

    virtual void configure(epics::pvData::PVStructure::shared_pointer configuration);
    virtual void flush();
    virtual void poll();

    virtual void destroy();

    /* ---------------------------------------------------------------- */

    void registerChannel(Channel::shared_pointer const & channel);
    void unregisterChannel(Channel::shared_pointer const & channel);

private:

    void initialize();

    epics::pvData::Mutex channelsMutex;
    // TODO std::unordered_map
    // void* is not the nicest thing, but there is no fast weak_ptr==
    typedef std::map<void*, Channel::weak_pointer> ChannelList;
    ChannelList channels;
};


class CAClientFactory
{
public:
    static void start();
    static void stop();
};

}}}

#endif  /* CAPROVIDER_H */
