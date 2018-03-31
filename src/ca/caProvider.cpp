/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <algorithm>

#include <cadef.h>
#include <epicsSignal.h>
#include <epicsThread.h>
#include <epicsExit.h>
#include <pv/logger.h>
#include <pv/configuration.h>
#include <pv/pvAccess.h>
#include <pv/reftrack.h>

#define epicsExportSharedSymbols
#include <pv/caProvider.h>
#include "caProviderPvt.h"
#include "caChannel.h"


namespace epics {
namespace pvAccess {
namespace ca {

using namespace epics::pvData;

#define EXCEPTION_GUARD(code) try { code; } \
        catch (std::exception &e) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d: %s", __FILE__, __LINE__, e.what()); } \
                catch (...) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d.", __FILE__, __LINE__); }

size_t CAChannelProvider::num_instances;

CAChannelProvider::CAChannelProvider() 
    : current_context(0)
{
    initialize();
}

CAChannelProvider::CAChannelProvider(const std::tr1::shared_ptr<Configuration>&)
    :  current_context(0)
{
    if(DEBUG_LEVEL>0) {
          std::cout<< "CAChannelProvider::CAChannelProvider\n";
    }
    // Ignoring Configuration as CA only allows config via. environment,
    // and we don't want to change this here.
    initialize();
}

CAChannelProvider::~CAChannelProvider()
{
    if(DEBUG_LEVEL>0) {
         std::cout << "CAChannelProvider::~CAChannelProvider()"
            << " caChannelList.size() " << caChannelList.size()
            << std::endl;
    }
    std::queue<CAChannelPtr> channelQ;
    {
         Lock lock(channelListMutex);
         for(size_t i=0; i< caChannelList.size(); ++i) {
             CAChannelPtr caChannel(caChannelList[i].lock());
             if(caChannel) channelQ.push(caChannel);
         }
         caChannelList.clear();
    }
    attachContext();
    while(!channelQ.empty()) {
       if(DEBUG_LEVEL>0) {
           std::cout << "disconnectAllChannels calling disconnectChannel "
                     << channelQ.front()->getChannelName()
                      << std::endl;
       }  
       channelQ.front()->disconnectChannel();
       channelQ.pop();
    }
    ca_flush_io();
    ca_context_destroy();
}

std::string CAChannelProvider::getProviderName()
{
    return "ca";
}

ChannelFind::shared_pointer CAChannelProvider::channelFind(
    std::string const & channelName,
    ChannelFindRequester::shared_pointer const & channelFindRequester)
{
    if (channelName.empty())
        throw std::invalid_argument("empty channel name");

    if (!channelFindRequester)
        throw std::invalid_argument("null requester");

    Status errorStatus(Status::STATUSTYPE_ERROR, "not implemented");
    ChannelFind::shared_pointer nullChannelFind;
    EXCEPTION_GUARD(channelFindRequester->channelFindResult(errorStatus, nullChannelFind, false));
    return nullChannelFind;
}

ChannelFind::shared_pointer CAChannelProvider::channelList(
    ChannelListRequester::shared_pointer const & channelListRequester)
{
    if (!channelListRequester.get())
        throw std::runtime_error("null requester");

    Status errorStatus(Status::STATUSTYPE_ERROR, "not implemented");
    ChannelFind::shared_pointer nullChannelFind;
    PVStringArray::const_svector none;
    EXCEPTION_GUARD(channelListRequester->channelListResult(errorStatus, nullChannelFind, none, false));
    return nullChannelFind;
}

Channel::shared_pointer CAChannelProvider::createChannel(
    std::string const & channelName,
    ChannelRequester::shared_pointer const & channelRequester,
    short priority)
{
    static std::string emptyString;
    Channel::shared_pointer channel(
        createChannel(channelName, channelRequester, priority, emptyString));
    return channel;
}

Channel::shared_pointer CAChannelProvider::createChannel(
    std::string const & channelName,
    ChannelRequester::shared_pointer const & channelRequester,
    short priority,
    std::string const & address)
{
    if (!address.empty())
        throw std::invalid_argument("CA does not support 'address' parameter");

    return CAChannel::create(shared_from_this(), channelName, priority, channelRequester);
}

void CAChannelProvider::addChannel(const CAChannelPtr & channel)
{
    if(DEBUG_LEVEL>0) {
         std::cout << "CAChannelProvider::addChannel "
             << channel->getChannelName()
             << std::endl;
    }
    Lock lock(channelListMutex);
    for(size_t i=0; i< caChannelList.size(); ++i) {
         if(!(caChannelList[i].lock())) {
             caChannelList[i] = channel;
             return;
         }
    }
    caChannelList.push_back(channel);
}

void CAChannelProvider::configure(epics::pvData::PVStructure::shared_pointer /*configuration*/)
{
}

void CAChannelProvider::flush()
{
}

void CAChannelProvider::poll()
{
}


void CAChannelProvider::attachContext()
{
    ca_client_context* thread_context = ca_current_context();
    if (thread_context == current_context) return;
    if (thread_context != NULL) {
        throw std::runtime_error("CAChannelProvider: Foreign CA context in use");
    }
    int result = ca_attach_context(current_context);
    if (result != ECA_NORMAL) {
        std::cout <<
            "CA error %s occurred while calling ca_attach_context:"
            << ca_message(result) << std::endl;
    }
}

void CAChannelProvider::initialize()
{
    if(DEBUG_LEVEL>0) std::cout << "CAChannelProvider::initialize()\n";
    /* Create Channel Access */
    int result = ca_context_create(ca_enable_preemptive_callback);
    if (result != ECA_NORMAL) {
        throw std::runtime_error(
            std::string("CA error %s occurred while trying to start channel access:")
            + ca_message(result));
    }
    current_context = ca_current_context();
}

void CAClientFactory::start()
{
    if(DEBUG_LEVEL>0) std::cout << "CAClientFactory::start()\n";
    if(ChannelProviderRegistry::clients()->getProvider("ca")) {
         // do not start twice
         return;
    }
    epicsSignalInstallSigAlarmIgnore();
    epicsSignalInstallSigPipeIgnore();
    registerRefCounter("CAChannelProvider", &CAChannelProvider::num_instances);
    registerRefCounter("CAChannel", &CAChannel::num_instances);
    registerRefCounter("CAChannelGet", &CAChannelGet::num_instances);
    registerRefCounter("CAChannelPut", &CAChannelPut::num_instances);
    registerRefCounter("CAChannelMonitor", &CAChannelMonitor::num_instances);

    if(!ChannelProviderRegistry::clients()->add<CAChannelProvider>("ca", true))
    {
         throw std::runtime_error("CAClientFactory::start failed");
    }
}

void CAClientFactory::stop()
{
    // unregister now done with exit hook
}


}
}
}

