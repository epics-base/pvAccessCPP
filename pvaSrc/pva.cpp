/* pva.cpp */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2015.02
 */

#define epicsExportSharedSymbols
#include <map>
#include <pv/pva.h>
#include <pv/createRequest.h>
#include <pv/clientFactory.h>
#include <pv/caProvider.h>

using std::tr1::static_pointer_cast;
using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::pvAccess::ca;
using namespace std;

namespace epics { namespace pva { 

static FieldCreatePtr fieldCreate = getFieldCreate(); 
static const string pvaName = "pva";
static const string defaultProvider = "pva";
static UnionConstPtr variantUnion = fieldCreate->createVariantUnion();

namespace pvaPvt {

    static size_t numberPva = 0;
    static bool firstTime = true;
    static Mutex mutex;
    
    class StartStopClientFactory {
    public:
        static void PvaBeingConstructed()
        {
            bool saveFirst = false;
            { 
                 Lock xx(mutex);
                 ++numberPva;
                 saveFirst = firstTime;
                 firstTime = false;
            }
            if(saveFirst) {
                ClientFactory::start();
                CAClientFactory::start();
            }
        }
    
        static void PvaBeingDestroyed() {
            size_t numLeft = 0;
            {
                 Lock xx(mutex);
                 --numberPva;
                  numLeft = numberPva;
            }
            if(numLeft<=0) {
                ClientFactory::stop();
                CAClientFactory::stop();
            }
        }
    };

} // namespace pvaPvt

class PvaChannelCache
{
public:
    PvaChannelCache(){}
    ~PvaChannelCache(){
         destroy();
     }
    void destroy() {
       pvaChannelMap.clear();
    }
    PvaChannelPtr getChannel(string const & channelName);
    void addChannel(PvaChannelPtr const & pvaChannel);
    void removeChannel(string const & channelName);
private:
    map<string,PvaChannelPtr> pvaChannelMap;
};
   
PvaChannelPtr PvaChannelCache::getChannel(string const & channelName)
{
    map<string,PvaChannelPtr>::iterator iter = pvaChannelMap.find(channelName);
    if(iter!=pvaChannelMap.end()) return iter->second;
    return PvaChannelPtr();
}

void PvaChannelCache::addChannel(PvaChannelPtr const & pvaChannel)
{
     pvaChannelMap.insert(std::pair<string,PvaChannelPtr>(
         pvaChannel->getChannelName(),pvaChannel));
}

void PvaChannelCache::removeChannel(string const & channelName)
{
    map<string,PvaChannelPtr>::iterator iter = pvaChannelMap.find(channelName);
    if(iter!=pvaChannelMap.end()) pvaChannelMap.erase(iter);
}

using namespace epics::pva::pvaPvt;

PvaPtr Pva::create()
{
    PvaPtr xx(new Pva());
    StartStopClientFactory::PvaBeingConstructed();
    return xx;
}

PVStructurePtr Pva::createRequest(string const &request)
{
    CreateRequest::shared_pointer createRequest = CreateRequest::create();
    PVStructurePtr pvRequest = createRequest->createRequest(request);
    if(!pvRequest) {
        throw std::invalid_argument("invalid pvRequest: " + createRequest->getMessage());
    }
    return pvRequest;
}

Pva::Pva()
:   pvaChannelCache(new PvaChannelCache()),
    isDestroyed(false)
{
}

Pva::~Pva() {
    destroy();
}

void Pva::destroy()
{
    {
        Lock xx(mutex);
        if(isDestroyed) return;
        isDestroyed = true;
    }
    pvaChannelCache.reset();
    channelList.clear();
    multiChannelList.clear();
    StartStopClientFactory::PvaBeingDestroyed();
}

string Pva:: getRequesterName()
{
    static string name("pva");
    return name;
}

void  Pva::message(
        string const & message,
        MessageType messageType)
{
    cout << getMessageTypeName(messageType) << " " << message << endl;
}

PvaChannelPtr Pva::channel(
        std::string const & channelName,
        std::string const & providerName,
        double timeOut)
{
    PvaChannelPtr pvaChannel = pvaChannelCache->getChannel(channelName);
    if(pvaChannel) return pvaChannel;
    pvaChannel = createChannel(channelName,providerName);
    pvaChannel->connect(timeOut);
    pvaChannelCache->addChannel(pvaChannel);
    return pvaChannel;
}

PvaChannelPtr Pva::createChannel(string const & channelName)
{
     return PvaChannel::create(getPtrSelf(),channelName);
}

PvaChannelPtr Pva::createChannel(string const & channelName, string const & providerName)
{
     return PvaChannel::create(getPtrSelf(),channelName,providerName);
}

PvaMultiChannelPtr Pva::createMultiChannel(
    epics::pvData::PVStringArrayPtr const & channelNames)
{
    return createMultiChannel(channelNames,"pva");
}

PvaMultiChannelPtr Pva::createMultiChannel(
    epics::pvData::PVStringArrayPtr const & channelNames,
    std::string const & providerName)
{
    return PvaMultiChannel::create(getPtrSelf(),channelNames,providerName);
}

}}

