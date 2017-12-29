/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef CAPROVIDERPVT_H
#define CAPROVIDERPVT_H

#include <cadef.h>

#include <pv/caProvider.h>
#include <pv/pvAccess.h>


namespace epics {
namespace pvAccess {
namespace ca {

#define DEBUG_LEVEL 0

class CAChannel;
typedef std::tr1::shared_ptr<CAChannel> CAChannelPtr;
typedef std::tr1::weak_ptr<CAChannel> CAChannelWPtr;

class CAChannelProvider;
typedef std::tr1::shared_ptr<CAChannelProvider> CAChannelProviderPtr;
typedef std::tr1::weak_ptr<CAChannelProvider> CAChannelProviderWPtr;

class CAChannelProvider :
    public ChannelProvider,
    public std::tr1::enable_shared_from_this<CAChannelProvider>
{
public:
    POINTER_DEFINITIONS(CAChannelProvider);

    static size_t num_instances;

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

    virtual void destroy() EPICS_DEPRECATED {};

    void addChannel(const CAChannelPtr & get);

    /* ---------------------------------------------------------------- */

    void threadAttach();
    

private:
    void initialize();
    ca_client_context* current_context;
    epics::pvData::Mutex channelListMutex;
    std::vector<CAChannelWPtr> caChannelList;
};

}
}
}

#endif  /* CAPROVIDERPVT_H */
