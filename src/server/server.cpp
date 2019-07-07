/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <epicsMutex.h>
#include <epicsGuard.h>

#include <pv/sharedPtr.h>
#include <pv/sharedVector.h>
#include <pv/pvData.h>
#include <pv/bitSet.h>
#include <pv/createRequest.h>
#include <pv/status.h>
#include <pv/reftrack.h>

#define epicsExportSharedSymbols
#include "pva/server.h"
#include "pv/pvAccess.h"
#include "pv/security.h"
#include "pv/reftrack.h"

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

namespace pvas {

struct StaticProvider::Impl : public pva::ChannelProvider
{
    POINTER_DEFINITIONS(Impl);

    static size_t num_instances;

    const std::string name;
    pva::ChannelFind::shared_pointer finder; // const after ctor
    std::tr1::weak_ptr<Impl> internal_self, external_self; // const after ctor

    mutable epicsMutex mutex;

    typedef StaticProvider::builders_t builders_t;
    builders_t builders;

    Impl(const std::string& name)
        :name(name)
    {
        REFTRACE_INCREMENT(num_instances);
    }
    virtual ~Impl() {
        REFTRACE_DECREMENT(num_instances);
    }

    virtual void destroy() OVERRIDE FINAL {}

    virtual std::string getProviderName() OVERRIDE FINAL { return name; }
    virtual pva::ChannelFind::shared_pointer channelFind(std::string const & name,
                                                         pva::ChannelFindRequester::shared_pointer const & requester) OVERRIDE FINAL
    {
        bool found;

        {
            Guard G(mutex);

            found =  builders.find(name)!=builders.end();
        }
        requester->channelFindResult(pvd::Status(), finder, found);
        return finder;
    }
    virtual pva::ChannelFind::shared_pointer channelList(pva::ChannelListRequester::shared_pointer const & requester) OVERRIDE FINAL
    {
        epics::pvData::PVStringArray::svector names;
        {
            Guard G(mutex);
            names.reserve(builders.size());
            for(builders_t::const_iterator it(builders.begin()), end(builders.end()); it!=end; ++it) {
                names.push_back(it->first);
            }
        }
        requester->channelListResult(pvd::Status(), finder, pvd::freeze(names), false);
        return finder;
    }
    virtual pva::Channel::shared_pointer createChannel(std::string const & name,
                                                       pva::ChannelRequester::shared_pointer const & requester,
                                                       short priority, std::string const & address) OVERRIDE FINAL
    {
        pva::Channel::shared_pointer ret;
        pvd::Status sts;

        builders_t::mapped_type builder;
        {
            Guard G(mutex);
            builders_t::const_iterator it(builders.find(name));
            if(it!=builders.end()) {
                UnGuard U(G);
                builder = it->second;
            }
        }
        if(builder)
            ret = builder->connect(Impl::shared_pointer(internal_self), name, requester);

        if(!ret) {
            sts = pvd::Status::error("No such channel");
        }

        requester->channelCreated(sts, ret);
        return ret;
    }

};

size_t StaticProvider::Impl::num_instances;

StaticProvider::ChannelBuilder::~ChannelBuilder() {}

StaticProvider::StaticProvider(const std::string &name)
    :impl(new Impl(name))
{
    impl->internal_self = impl;
    impl->finder = pva::ChannelFind::buildDummy(impl);
    // wrap ref to call destroy when all external refs (from DyamicProvider::impl) are released.
    impl.reset(impl.get(), pva::Destroyable::cleaner(impl));
    impl->external_self = impl;
}

StaticProvider::~StaticProvider() { close(true); }

void StaticProvider::close(bool destroy)
{
    Impl::builders_t pvs;
    {
        Guard G(impl->mutex);
        if(destroy) {
            pvs.swap(impl->builders); // consume
        } else {
            pvs = impl->builders; // just copy, close() is a relatively rare action
        }
    }
    for(Impl::builders_t::iterator it(pvs.begin()), end(pvs.end()); it!=end; ++it) {
        it->second->disconnect(destroy, impl.get());
    }
}

std::tr1::shared_ptr<epics::pvAccess::ChannelProvider> StaticProvider::provider() const
{
    return Impl::shared_pointer(impl->internal_self);
}

void StaticProvider::add(const std::string& name,
         const std::tr1::shared_ptr<ChannelBuilder>& builder)
{
    Guard G(impl->mutex);
    if(impl->builders.find(name)!=impl->builders.end())
        throw std::logic_error("Duplicate PV name");
    impl->builders[name] = builder;
}

std::tr1::shared_ptr<StaticProvider::ChannelBuilder> StaticProvider::remove(const std::string& name)
{
    std::tr1::shared_ptr<StaticProvider::ChannelBuilder> ret;
    {
        Guard G(impl->mutex);
        Impl::builders_t::iterator it(impl->builders.find(name));
        if(it!=impl->builders.end()) {
            ret = it->second;
            impl->builders.erase(it);
        }
    }
    if(ret)
        ret->disconnect(true, impl.get());
    return ret;
}

StaticProvider::builders_t::const_iterator StaticProvider::begin() const {
    Guard G(impl->mutex);
    return impl->builders.begin();
}

StaticProvider::builders_t::const_iterator StaticProvider::end() const {
    Guard G(impl->mutex);
    return impl->builders.end();
}


struct DynamicProvider::Impl : public pva::ChannelProvider
{
    POINTER_DEFINITIONS(Impl);

    static size_t num_instances;

    const std::string name;
    const std::tr1::shared_ptr<Handler> handler;
    pva::ChannelFind::shared_pointer finder; // const after ctor
    std::tr1::weak_ptr<Impl> internal_self, external_self; // const after ctor

    mutable epicsMutex mutex;

    Impl(const std::string& name,
         const std::tr1::shared_ptr<Handler>& handler)
        :name(name)
        ,handler(handler)
    {
        REFTRACE_INCREMENT(num_instances);
    }
    virtual ~Impl() {
        REFTRACE_DECREMENT(num_instances);
    }

    virtual void destroy() OVERRIDE FINAL {
        handler->destroy();
    }

    virtual std::string getProviderName() OVERRIDE FINAL { return name; }
    virtual pva::ChannelFind::shared_pointer channelFind(std::string const & name,
                                                         pva::ChannelFindRequester::shared_pointer const & requester) OVERRIDE FINAL
    {
        bool found = false;
        {
            pva::PeerInfo::const_shared_pointer info(requester->getPeerInfo());
            search_type search;
            search.push_back(DynamicProvider::Search(name, info ? info.get() : 0));

            handler->hasChannels(search);

            found = !search.empty() && search[0].name()==name && search[0].claimed();
        }
        requester->channelFindResult(pvd::Status(), finder, found);
        return finder;
    }
    virtual pva::ChannelFind::shared_pointer channelList(pva::ChannelListRequester::shared_pointer const & requester) OVERRIDE FINAL
    {
        epics::pvData::PVStringArray::svector names;
        bool dynamic = true;
        handler->listChannels(names, dynamic);
        requester->channelListResult(pvd::Status(), finder, pvd::freeze(names), dynamic);
        return finder;
    }
    virtual pva::Channel::shared_pointer createChannel(std::string const & name,
                                                       pva::ChannelRequester::shared_pointer const & requester,
                                                       short priority, std::string const & address) OVERRIDE FINAL
    {
        pva::Channel::shared_pointer ret;
        pvd::Status sts;

        ret = handler->createChannel(ChannelProvider::shared_pointer(internal_self), name, requester);
        if(!ret)
            sts = pvd::Status::error("Channel no longer available"); // because we only get here if channelFind() succeeds

        requester->channelCreated(sts, ret);
        return ret;
    }

};

size_t DynamicProvider::Impl::num_instances;

DynamicProvider::DynamicProvider(const std::string &name,
                                 const std::tr1::shared_ptr<Handler> &handler)
    :impl(new Impl(name, handler))
{
    impl->internal_self = impl;
    impl->finder = pva::ChannelFind::buildDummy(impl);
    // wrap ref to call destroy when all external refs (from DyamicProvider::impl) are released.
    impl.reset(impl.get(), pva::Destroyable::cleaner(impl));
    impl->external_self = impl;
}

DynamicProvider::~DynamicProvider() {}

DynamicProvider::Handler::shared_pointer DynamicProvider::getHandler() const
{
    return impl->handler;
}

std::tr1::shared_ptr<epics::pvAccess::ChannelProvider> DynamicProvider::provider() const
{
    return Impl::shared_pointer(impl->internal_self);
}

void registerRefTrackServer()
{
    epics::registerRefCounter("pvas::StaticProvider", &StaticProvider::Impl::num_instances);
    epics::registerRefCounter("pvas::DynamicProvider", &DynamicProvider::Impl::num_instances);
}

} // namespace pvas
