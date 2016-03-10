/* syncChannelFind.h */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author Marty Kraimer
 * @date 2014.02
 */
/**
 * This is an implementation of ChannelFind that is appropriate for all channel
 * providers that can synchronously determine if the provider has the channel.
 */
#ifndef SYNCCHANNELFIND_H
#define SYNCCHANNELFIND_H

#include <string>
#include <cstring>
#include <stdexcept>
#include <memory>

#ifdef epicsExportSharedSymbols
#   define syncChannelFindEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif
#include <pv/pvData.h>
#ifdef syncChannelFindEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   undef syncChannelFindEpicsExportSharedSymbols
#endif

#include <shareLib.h>
#include <pv/pvAccess.h>


namespace epics {
namespace pvAccess {

class SyncChannelFind : public ChannelFind
{
public:
    typedef std::tr1::shared_ptr<SyncChannelFind> shared_pointer;

    SyncChannelFind(ChannelProvider::shared_pointer &provider) : m_provider(provider)
    {
    }

    virtual ~SyncChannelFind() {}

    virtual void destroy() {}

    virtual ChannelProvider::shared_pointer getChannelProvider()
    {
        return m_provider.lock();
    };

    virtual void cancel() {}

private:
    ChannelProvider::weak_pointer m_provider;
};




}
}
#endif  /* SYNCCHANNELFIND_H */
