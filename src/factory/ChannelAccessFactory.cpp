/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <map>
#include <vector>

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

ChannelProviderRegistry::ChannelProviderRegistry()
{
std::cout << "ChannelProviderRegistry\n";
}

ChannelProviderRegistry::~ChannelProviderRegistry()
{
std::cout << "~ChannelProviderRegistry\n";
}


ChannelProviderRegistry::shared_pointer ChannelProviderRegistry::getChannelProviderRegistry()
{
    static Mutex mutex;
    static ChannelProviderRegistry::shared_pointer global_reg;
    Lock guard(mutex);
    if(!global_reg) {
        global_reg = ChannelProviderRegistry::build();
    }
    return global_reg;
}


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
    Lock G(mutex);
    std::string name(fact->getFactoryName());
    if(!replace && providers.find(name)!=providers.end())
        throw false;
    providers[name] = fact;
    return true;
}

ChannelProviderFactory::shared_pointer ChannelProviderRegistry::remove(const std::string& name)

{
    ChannelProviderFactory::shared_pointer ret;
    providers_t::iterator iter(providers.find(name));
    if(iter!=providers.end()) {
        ret = iter->second;
        providers.erase(iter);
    }
    return ret;
}

ChannelProviderRegistry::shared_pointer getChannelProviderRegistry() {
std::cerr << "getChannelProviderRegistry should not be used\n";
    return ChannelProviderRegistry::getChannelProviderRegistry();
}

void registerChannelProviderFactory(ChannelProviderFactory::shared_pointer const & channelProviderFactory) {
std::cerr << "registerChannelProviderFactory should not be used\n";
     getChannelProviderRegistry()->add(channelProviderFactory);
}

void unregisterChannelProviderFactory(ChannelProviderFactory::shared_pointer const & channelProviderFactory) {
std::cerr << "unregisterChannelProviderFactory should not be used\n";
    getChannelProviderRegistry()->remove(channelProviderFactory->getFactoryName());
}

epicsShareFunc void unregisterAllChannelProviderFactory()
{
    getChannelProviderRegistry()->clear();
}


}
}

