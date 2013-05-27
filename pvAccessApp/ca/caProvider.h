/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef CAPROVIDER_H
#define CAPROVIDER_H

#include <pv/pvAccess.h>

namespace epics {
namespace pvAccess {

class CAChannelProvider :
        public ChannelProvider,
        public std::tr1::enable_shared_from_this<CAChannelProvider>
{
public:

    CAChannelProvider();
    virtual ~CAChannelProvider();

    virtual epics::pvData::String getProviderName();

    virtual void destroy();

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

private:

    void initialize();
};

extern ChannelProvider::shared_pointer createCAChannelProvider();


}}

#endif  /* CAPROVIDER_H */
