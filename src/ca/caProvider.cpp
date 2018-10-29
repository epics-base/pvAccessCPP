/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <cadef.h>
#include <epicsSignal.h>
#include <epicsThread.h>
#include <epicsExit.h>
#include <pv/logger.h>
#include <pv/pvAccess.h>

#include "channelConnectThread.h"
#include "monitorEventThread.h"
#include "getDoneThread.h"
#include "putDoneThread.h"

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

CAChannelProvider::CAChannelProvider() 
    : current_context(0)
{
    initialize();
}

CAChannelProvider::CAChannelProvider(const std::tr1::shared_ptr<Configuration>&)
    :  current_context(0),
       channelConnectThread(ChannelConnectThread::get()),
       monitorEventThread(MonitorEventThread::get()),
       getDoneThread(GetDoneThread::get()),
       putDoneThread(PutDoneThread::get())
{
    if(DEBUG_LEVEL>0) {
          std::cout<< "CAChannelProvider::CAChannelProvider\n";
    }
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
         for(size_t i=0; i< caChannelList.size(); ++i)
         {
             CAChannelPtr caChannel(caChannelList[i].lock());
             if(caChannel) channelQ.push(caChannel);
         }
         caChannelList.clear();
    }
    while(!channelQ.empty()) {
       if(DEBUG_LEVEL>0) {
           std::cout << "~CAChannelProvider() calling disconnectChannel "
                     << channelQ.front()->getChannelName()
                      << std::endl;
       }  
       channelQ.front()->disconnectChannel();
       channelQ.pop();
    }
    putDoneThread->stop();
    getDoneThread->stop();
    monitorEventThread->stop();
    channelConnectThread->stop();
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelProvider::~CAChannelProvider() calling ca_context_destroy\n";
    }
    ca_context_destroy();
//std::cout << "CAChannelProvider::~CAChannelProvider() returning\n";
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
        throw std::invalid_argument("CAChannelProvider::channelFind empty channel name");

    if (!channelFindRequester)
        throw std::invalid_argument("CAChannelProvider::channelFind null requester");

    Status errorStatus(Status::STATUSTYPE_ERROR, "CAChannelProvider::channelFind not implemented");
    ChannelFind::shared_pointer nullChannelFind;
    EXCEPTION_GUARD(channelFindRequester->channelFindResult(errorStatus, nullChannelFind, false));
    return nullChannelFind;
}

ChannelFind::shared_pointer CAChannelProvider::channelList(
    ChannelListRequester::shared_pointer const & channelListRequester)
{
    if (!channelListRequester.get())
        throw std::runtime_error("CAChannelProvider::channelList null requester");

    Status errorStatus(Status::STATUSTYPE_ERROR, "CAChannelProvider::channelList not implemented");
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
    Channel::shared_pointer channel(
        createChannel(channelName, channelRequester, priority, std::string()));
    return channel;
}

Channel::shared_pointer CAChannelProvider::createChannel(
    std::string const & channelName,
    ChannelRequester::shared_pointer const & channelRequester,
    short priority,
    std::string const & address)
{
    if (!address.empty())
        throw std::invalid_argument("CAChannelProvider::createChannel does not support 'address' parameter");

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
    int result = ca_attach_context(current_context);
    if(result==ECA_ISATTACHED) return;
    if (result != ECA_NORMAL) {
        std::string mess("CAChannelProvider::attachContext error  calling ca_attach_context ");
        mess += ca_message(result);
        throw std::runtime_error(mess);
    }
}

void CAChannelProvider::initialize()
{
    if(DEBUG_LEVEL>0) std::cout << "CAChannelProvider::initialize()\n";
    int result = ca_context_create(ca_enable_preemptive_callback);
    if (result != ECA_NORMAL) {
        std::string mess("CAChannelProvider::initialize error calling ca_context_create ");
        mess += ca_message(result);
        throw std::runtime_error(mess);
    }
    current_context = ca_current_context();
}

void CAClientFactory::start()
{
    if(DEBUG_LEVEL>0) std::cout << "CAClientFactory::start()\n";
    if(ChannelProviderRegistry::clients()->getProvider("ca")) {
         return;
    }
    epicsSignalInstallSigAlarmIgnore();
    epicsSignalInstallSigPipeIgnore();
    if(!ChannelProviderRegistry::clients()->add<CAChannelProvider>("ca", true))
    {
         throw std::runtime_error("CAClientFactory::start failed");
    }
}

void CAClientFactory::stop()
{
    // unregister now done with exit hook
}

}}}

