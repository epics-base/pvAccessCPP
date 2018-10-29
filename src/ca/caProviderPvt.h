/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

/**
 * @author msekoranja, mrk
 * @date 2018.07
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

class ChannelConnectThread;
typedef std::tr1::shared_ptr<ChannelConnectThread> ChannelConnectThreadPtr;

class MonitorEventThread;
typedef std::tr1::shared_ptr<MonitorEventThread> MonitorEventThreadPtr;

class GetDoneThread;
typedef std::tr1::shared_ptr<GetDoneThread> GetDoneThreadPtr;

class PutDoneThread;
typedef std::tr1::shared_ptr<PutDoneThread> PutDoneThreadPtr;

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

    void attachContext();
    void addChannel(const CAChannelPtr & channel);
private:
    
    virtual void destroy() EPICS_DEPRECATED {}
    void initialize();
    ca_client_context* current_context;
    epics::pvData::Mutex channelListMutex;
    std::vector<CAChannelWPtr> caChannelList;
    ChannelConnectThreadPtr channelConnectThread;
    MonitorEventThreadPtr monitorEventThread;
    GetDoneThreadPtr getDoneThread;
    PutDoneThreadPtr putDoneThread;
};

}}}

#endif  /* CAPROVIDERPVT_H */
