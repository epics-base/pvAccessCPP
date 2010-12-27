/*ChannelAccessFactory.cpp*/

#include <lock.h>
#include <noDefaultMethods.h>
#include <pvAccess.h>
#include <pvData.h>
#include <factory.h>
#include <map>
#include <vector>

using namespace epics::pvData;

namespace epics { namespace pvAccess {

static ChannelAccess* channelAccess = 0;

static Mutex channelProviderMutex = Mutex();

typedef std::map<String, ChannelProvider*> ChannelProviderMap;
static ChannelProviderMap channelProviders;


class ChannelAccessImpl : public ChannelAccess {
    public:

    ChannelProvider* getProvider(String providerName) {
        Lock guard(&channelProviderMutex);
        return channelProviders[providerName];
    }

    std::vector<String>* getProviderNames() {
        Lock guard(&channelProviderMutex);
        std::vector<String>* providers = new std::vector<String>();
        for (ChannelProviderMap::const_iterator i = channelProviders.begin();
            i != channelProviders.end(); i++)
            providers->push_back(i->first);

        return providers;
    }
};

ChannelAccess * getChannelAccess() {
    static Mutex mutex = Mutex();
    Lock guard(&mutex);

    if(channelAccess==0){
        channelAccess = new ChannelAccessImpl();
    }
    return channelAccess;
}

void registerChannelProvider(ChannelProvider *channelProvider) {
    Lock guard(&channelProviderMutex);
    channelProviders[channelProvider->getProviderName()] = channelProvider;
}

void unregisterChannelProvider(ChannelProvider *channelProvider) {
    Lock guard(&channelProviderMutex);
    channelProviders.erase(channelProvider->getProviderName());
}

}}

