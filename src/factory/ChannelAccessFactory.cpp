/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <map>
#include <vector>

#include <osiSock.h>
#include <epicsThread.h>
#include <epicsSignal.h>

#include <pv/lock.h>
#include <pv/noDefaultMethods.h>
#include <pv/pvData.h>
#include <pv/reftrack.h>
#include <pv/timer.h>

#define epicsExportSharedSymbols
#include <pv/pvAccess.h>
#include <pv/clientContextImpl.h>
#include <pv/factory.h>
#include "pv/codec.h"
#include <pv/serverContextImpl.h>
#include <pv/serverChannelImpl.h>
#include <pv/blockingUDP.h>
#include <sharedstateimpl.h>

using namespace epics::pvData;
using std::string;

namespace pvas {
void registerRefTrackServer();
}

namespace epics {
namespace pvAccess {

ChannelProviderRegistry::shared_pointer ChannelProviderRegistry::build() {
    ChannelProviderRegistry::shared_pointer ret(new ChannelProviderRegistry);
    return ret;
}

ChannelProvider::shared_pointer ChannelProviderRegistry::getProvider(std::string const & providerName) {
    ChannelProviderFactory::shared_pointer fact(getFactory(providerName));
    if(fact)
        return fact->sharedInstance();
    else
        return ChannelProvider::shared_pointer();
}

ChannelProvider::shared_pointer ChannelProviderRegistry::createProvider(std::string const & providerName,
                                                                        const std::tr1::shared_ptr<Configuration>& conf) {
    ChannelProviderFactory::shared_pointer fact(getFactory(providerName));
    if(fact)
        return fact->newInstance(conf);
    else
        return ChannelProvider::shared_pointer();
}

ChannelProviderFactory::shared_pointer ChannelProviderRegistry::getFactory(std::string const & providerName)
{
    Lock G(mutex);
    providers_t::const_iterator iter = providers.find(providerName);
    if (iter == providers.end())
        return ChannelProviderFactory::shared_pointer();
    else
        return iter->second;
}

void ChannelProviderRegistry::getProviderNames(std::set<std::string>& names)
{
    Lock G(mutex);
    for (providers_t::const_iterator iter = providers.begin();
         iter != providers.end(); iter++)
        names.insert(iter->first);
}


bool ChannelProviderRegistry::add(const ChannelProviderFactory::shared_pointer& fact, bool replace)
{
    assert(fact);
    Lock G(mutex);
    std::string name(fact->getFactoryName());
    if(!replace && providers.find(name)!=providers.end())
        return false;
    providers[name] = fact;
    return true;
}

namespace {
struct FunctionFactory : public ChannelProviderFactory {
    const std::string pname;
    epics::pvData::Mutex sharedM;
    ChannelProvider::weak_pointer shared;
    const ChannelProviderRegistry::factoryfn_t fn;

    FunctionFactory(const std::string& name, ChannelProviderRegistry::factoryfn_t fn)
        :pname(name), fn(fn)
    {}
    virtual ~FunctionFactory() {}
    virtual std::string getFactoryName() { return pname; }

    virtual ChannelProvider::shared_pointer sharedInstance()
    {
        epics::pvData::Lock L(sharedM);
        ChannelProvider::shared_pointer ret(shared.lock());
        if(!ret) {
            ret = fn(std::tr1::shared_ptr<Configuration>());
            shared = ret;
        }
        return ret;
    }

    virtual ChannelProvider::shared_pointer newInstance(const std::tr1::shared_ptr<Configuration>& conf)
    {
        return fn(conf);
    }

};
}//namespace

ChannelProviderFactory::shared_pointer ChannelProviderRegistry::add(const std::string& name, factoryfn_t fn, bool replace)
{
    ChannelProviderFactory::shared_pointer F(new FunctionFactory(name, fn));
    return add(F, replace) ? F : ChannelProviderFactory::shared_pointer();
}

namespace {
//! A ChannelProviderFactory based around a single pre-created ChannelProvider instance.
//! Only a weak ref to this instance is kept, so the instance must be kept active
//! through some external means
struct InstanceChannelProviderFactory : public ChannelProviderFactory
{
    InstanceChannelProviderFactory(const std::tr1::shared_ptr<ChannelProvider>& provider)
        :name(provider->getProviderName())
        ,provider(provider)
    {}
    virtual ~InstanceChannelProviderFactory() {}
    virtual std::string getFactoryName() OVERRIDE FINAL
    {
        return name;
    }
    virtual ChannelProvider::shared_pointer sharedInstance() OVERRIDE FINAL
    {
        return provider.lock();
    }
    virtual ChannelProvider::shared_pointer newInstance(const std::tr1::shared_ptr<Configuration>& conf) OVERRIDE FINAL
    {
        return provider.lock();
    }
private:
    const std::string name;
    const ChannelProvider::weak_pointer provider;
};
}//namespace

ChannelProviderFactory::shared_pointer ChannelProviderRegistry::addSingleton(const ChannelProvider::shared_pointer& provider,
                                                                             bool replace)
{
    std::tr1::shared_ptr<InstanceChannelProviderFactory> F(new InstanceChannelProviderFactory(provider));
    return add(F, replace) ? F : std::tr1::shared_ptr<InstanceChannelProviderFactory>();
}

ChannelProviderFactory::shared_pointer ChannelProviderRegistry::remove(const std::string& name)
{
    Lock G(mutex);
    ChannelProviderFactory::shared_pointer fact(getFactory(name));
    if(fact) {
        remove(fact);
    }
    return fact;
}

bool ChannelProviderRegistry::remove(const ChannelProviderFactory::shared_pointer& fact)
{
    assert(fact);
    Lock G(mutex);
    providers_t::iterator iter(providers.find(fact->getFactoryName()));
    if(iter!=providers.end() && iter->second==fact) {
        providers.erase(iter);
        return true;
    }
    return false;
}

void ChannelProviderRegistry::clear()
{
    Lock G(mutex);
    providers.clear();
}

namespace {
struct providerRegGbl_t {
    ChannelProviderRegistry::shared_pointer clients,
                                            servers;
    providerRegGbl_t()
        :clients(ChannelProviderRegistry::build())
        ,servers(ChannelProviderRegistry::build())
    {}
} *providerRegGbl;

epicsThreadOnceId providerRegOnce = EPICS_THREAD_ONCE_INIT;

} // namespace

void providerRegInit(void*)
{
    epicsSignalInstallSigAlarmIgnore();
    epicsSignalInstallSigPipeIgnore();

    providerRegGbl = new providerRegGbl_t;
    providerRegGbl->clients->add("pva", createClientProvider);

    registerRefCounter("ServerContextImpl", &ServerContextImpl::num_instances);
    registerRefCounter("ServerChannel", &ServerChannel::num_instances);
    registerRefCounter("Transport (ABC)", &Transport::num_instances);
    registerRefCounter("BlockingTCPTransportCodec", &detail::BlockingTCPTransportCodec::num_instances);
    registerRefCounter("BlockingUDPTransport", &BlockingUDPTransport::num_instances);
    registerRefCounter("ChannelProvider (ABC)", &ChannelProvider::num_instances);
    registerRefCounter("Channel (ABC)", &Channel::num_instances);
    registerRefCounter("ChannelRequester (ABC)", &ChannelRequester::num_instances);
    registerRefCounter("ChannelBaseRequester (ABC)", &ChannelBaseRequester::num_instances);
    registerRefCounter("ChannelRequest (ABC)", &ChannelRequest::num_instances);
    registerRefCounter("ResponseHandler (ABC)", &ResponseHandler::num_instances);
    registerRefCounter("MonitorFIFO", &MonitorFIFO::num_instances);
    pvas::registerRefTrackServer();
    registerRefCounter("pvas::SharedChannel", &pvas::detail::SharedChannel::num_instances);
    registerRefCounter("pvas::SharedPut", &pvas::detail::SharedPut::num_instances);
    registerRefCounter("pvas::SharedRPC", &pvas::detail::SharedRPC::num_instances);
    registerRefCounter("pvas::SharedPV", &pvas::SharedPV::num_instances);
}

ChannelProviderRegistry::shared_pointer ChannelProviderRegistry::clients()
{
    epicsThreadOnce(&providerRegOnce, &providerRegInit, 0);

    return providerRegGbl->clients;
}

ChannelProviderRegistry::shared_pointer ChannelProviderRegistry::servers()
{
    epicsThreadOnce(&providerRegOnce, &providerRegInit, 0);

    return providerRegGbl->servers;
}

ChannelFind::shared_pointer
ChannelProvider::channelList(ChannelListRequester::shared_pointer const & requester)
{
    ChannelFind::shared_pointer ret;
    requester->channelListResult(Status::error("not implemented"),
                                 ret,
                                 epics::pvData::PVStringArray::const_svector(),
                                 false);
    return ret;
}

Channel::shared_pointer
ChannelProvider::createChannel(std::string const & name,
                               ChannelRequester::shared_pointer const & requester,
                               short priority)
{
    return createChannel(name, requester, priority, "");
}

}
}

