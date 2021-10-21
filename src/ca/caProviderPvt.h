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
#include <epicsMutex.h>
#include <tsDLList.h>

#include <pv/logger.h>
#include <pv/pvAccess.h>

#include <pv/caProvider.h>
#include "caContext.h"
#include "notifierConveyor.h"


namespace epics {
namespace pvAccess {
namespace ca {

#define EXCEPTION_GUARD(code) try { code; } \
    catch (std::exception &e) { \
        LOG(logLevelError, "Unhandled exception from client code at %s:%d: %s", \
            __FILE__, __LINE__, e.what()); \
    } \
    catch (...) { \
        LOG(logLevelError, "Unhandled exception from client code at %s:%d.", \
            __FILE__, __LINE__); \
    }

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

    void addChannel(CAChannel &channel);
    void delChannel(CAChannel &channel);

    CAContextPtr caContext() {
        return ca_context;
    }
    void notifyConnection(NotificationPtr const &notificationPtr) {
        connectNotifier.notifyClient(notificationPtr);
    }
    void notifyResult(NotificationPtr const &notificationPtr) {
        resultNotifier.notifyClient(notificationPtr);
    }
private:
    CAContextPtr ca_context;
    epicsMutex channelListMutex;
    tsDLList<CAChannel> caChannelList;

    NotifierConveyor connectNotifier;
    NotifierConveyor resultNotifier;
};

}}}

#endif  /* CAPROVIDERPVT_H */
