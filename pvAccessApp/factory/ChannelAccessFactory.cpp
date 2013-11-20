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

namespace epics {
namespace pvAccess {

static ChannelAccess::shared_pointer channelAccess;

static Mutex channelProviderMutex;

typedef std::map<String, ChannelProviderFactory::shared_pointer> ChannelProviderFactoryMap;
static ChannelProviderFactoryMap channelProviders;


class ChannelAccessImpl : public ChannelAccess {
    public:

    ChannelProvider::shared_pointer getProvider(String const & _providerName) {
        
        // TODO remove, here for backward compatibility 
        const String providerName = (_providerName == "pvAccess") ? "pva" : _providerName;
            
        Lock guard(channelProviderMutex);
        ChannelProviderFactoryMap::const_iterator iter = channelProviders.find(providerName);
        if (iter != channelProviders.end())
            return iter->second->sharedInstance();
        else
            return ChannelProvider::shared_pointer();
    }

    ChannelProvider::shared_pointer createProvider(String const & _providerName) {

        // TODO remove, here for backward compatibility 
        const String providerName = (_providerName == "pvAccess") ? "pva" : _providerName;
            
        Lock guard(channelProviderMutex);
        ChannelProviderFactoryMap::const_iterator iter = channelProviders.find(providerName);
        if (iter != channelProviders.end())
            return iter->second->newInstance();
        else
            return ChannelProvider::shared_pointer();
    }

    std::auto_ptr<stringVector_t> getProviderNames() {
        Lock guard(channelProviderMutex);
        std::auto_ptr<stringVector_t> providers(new stringVector_t());
        for (ChannelProviderFactoryMap::const_iterator i = channelProviders.begin();
            i != channelProviders.end(); i++)
            providers->push_back(i->first);

        return providers;
    }
};

ChannelAccess::shared_pointer getChannelAccess() {
    static Mutex mutex;
    Lock guard(mutex);

    if(channelAccess.get()==0){
        channelAccess.reset(new ChannelAccessImpl());
    }
    return channelAccess;
}

void registerChannelProviderFactory(ChannelProviderFactory::shared_pointer const & channelProviderFactory) {
    Lock guard(channelProviderMutex);
    channelProviders[channelProviderFactory->getFactoryName()] = channelProviderFactory;
}

void unregisterChannelProviderFactory(ChannelProviderFactory::shared_pointer const & channelProviderFactory) {
    Lock guard(channelProviderMutex);
    channelProviders.erase(channelProviderFactory->getFactoryName());
}

}}

