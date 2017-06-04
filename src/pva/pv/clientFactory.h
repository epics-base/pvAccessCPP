/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef CLIENTFACTORY_H
#define CLIENTFACTORY_H

#include <shareLib.h>

#include <pv/sharedPtr.h>

namespace epics {
namespace pvAccess {

class ChannelProviderRegistry;
typedef std::tr1::shared_ptr<ChannelProviderRegistry> ChannelProviderRegistryPtr;

class ChannelProviderFactoryImpl;
typedef std::tr1::shared_ptr<ChannelProviderFactoryImpl> ChannelProviderFactoryImplPtr;

class epicsShareClass ClientFactory {
private:
    static ChannelProviderRegistryPtr channelRegistry;
    static ChannelProviderFactoryImplPtr channelProvider;
    static int numStart;
public:
    static void start();
    static void stop();
};

}
}

#endif  /* CLIENTFACTORY_H */
