/*ChannelAccessFactory.cpp*/

#include <pv/lock.h>
#include <pv/noDefaultMethods.h>
#include <pv/pvAccess.h>
#include <pv/pvData.h>
#include <pv/factory.h>
#include <map>
#include <vector>

using namespace epics::pvData;

namespace epics { namespace pvAccess {

static ChannelAccess::shared_pointer channelAccess;

static Mutex channelProviderMutex;

typedef std::map<String, ChannelProvider::shared_pointer> ChannelProviderMap;
static ChannelProviderMap channelProviders;


class ChannelAccessImpl : public ChannelAccess {
    public:

    ChannelProvider::shared_pointer getProvider(String providerName) {
        Lock guard(channelProviderMutex);
        return channelProviders[providerName];
    }

    std::auto_ptr<stringVector_t> getProviderNames() {
        Lock guard(channelProviderMutex);
        std::auto_ptr<stringVector_t> providers(new stringVector_t());
        for (ChannelProviderMap::const_iterator i = channelProviders.begin();
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

void registerChannelProvider(ChannelProvider::shared_pointer const & channelProvider) {
    Lock guard(channelProviderMutex);
    channelProviders[channelProvider->getProviderName()] = channelProvider;
}

void unregisterChannelProvider(ChannelProvider::shared_pointer const & channelProvider) {
    Lock guard(channelProviderMutex);
    channelProviders.erase(channelProvider->getProviderName());
}

}}

