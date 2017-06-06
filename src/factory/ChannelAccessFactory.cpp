/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <map>
#include <vector>

#include <epicsThread.h>

#include <pv/lock.h>
#include <pv/noDefaultMethods.h>
#include <pv/pvData.h>

#define epicsExportSharedSymbols
#include <pv/pvAccess.h>
#include <pv/factory.h>

using namespace epics::pvData;
using std::string;

namespace epics {
namespace pvAccess {

ChannelProvider::shared_pointer ChannelProviderRegistry::getProvider(std::string const & providerName) {
    ChannelProviderFactory::shared_pointer fact(getFactory(providerName));
    if(fact)
        return fact->sharedInstance();
    else
        return ChannelProvider::shared_pointer();
}

ChannelProvider::shared_pointer ChannelProviderRegistry::createProvider(std::string const & providerName) {
    ChannelProviderFactory::shared_pointer fact(getFactory(providerName));
    if(fact)
        return fact->newInstance();
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

std::auto_ptr<ChannelProviderRegistry::stringVector_t> ChannelProviderRegistry::getProviderNames()
{
    Lock G(mutex);
    std::auto_ptr<stringVector_t> ret(new stringVector_t);
    for (providers_t::const_iterator iter = providers.begin();
            iter != providers.end(); iter++)
        ret->push_back(iter->first);

    return ret;
}

bool ChannelProviderRegistry::add(const ChannelProviderFactory::shared_pointer& fact, bool replace)
{
    assert(fact);
    Lock G(mutex);
    std::string name(fact->getFactoryName());
    if(!replace && providers.find(name)!=providers.end())
        throw false;
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

ChannelProviderFactory::shared_pointer ChannelProviderRegistry::remove(const std::string& name)
{
    Lock G(mutex);
    ChannelProviderFactory::shared_pointer ret;
    providers_t::iterator iter(providers.find(name));
    if(iter!=providers.end()) {
        ret = iter->second;
        providers.erase(iter);
    }
    return ret;
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

namespace {
struct providerRegGbl_t {
    Mutex mutex;
    ChannelProviderRegistry::shared_pointer reg;
} *providerRegGbl;

epicsThreadOnceId providerRegOnce = EPICS_THREAD_ONCE_INIT;

void providerRegInit(void*)
{
    providerRegGbl = new providerRegGbl_t;
}

} // namespace

ChannelProviderRegistry::shared_pointer getChannelProviderRegistry()
{
    epicsThreadOnce(&providerRegOnce, &providerRegInit, 0);

    Lock guard(providerRegGbl->mutex);

    if(!providerRegGbl->reg) {
        providerRegGbl->reg = ChannelProviderRegistry::build();
    }
    return providerRegGbl->reg;
}

void registerChannelProviderFactory(ChannelProviderFactory::shared_pointer const & channelProviderFactory) {
    assert(channelProviderFactory);
    getChannelProviderRegistry()->add(channelProviderFactory);
}

void unregisterChannelProviderFactory(ChannelProviderFactory::shared_pointer const & channelProviderFactory) {
    assert(channelProviderFactory);
    getChannelProviderRegistry()->remove(channelProviderFactory->getFactoryName());
}

epicsShareFunc void unregisterAllChannelProviderFactory()
{
    getChannelProviderRegistry()->clear();
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

