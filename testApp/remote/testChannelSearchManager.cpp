/* testChannelSearcManager.cpp */

#include <epicsExit.h>
#include <channelSearchManager.h>
#include <sstream>
#include <CDRMonitor.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

//TODO this will be deleted
class ChannelImpl;

class ContextImpl : public Context
{
public:
	ContextImpl(): _timer(new Timer("krneki",lowPriority))
	{

	}
	virtual Version* getVersion()
	{
		return NULL;
	}
	virtual ChannelProvider* getProvider()
	{
		return NULL;
	}
	Timer::shared_pointer getTimer()
	{
		return _timer;
	}
	virtual void initialize()
	{
	}
	virtual void printInfo()
	{
	}
	virtual void printInfo(epics::pvData::StringBuilder out)
	{
	}
	virtual void destroy()
	{
	}
	virtual void dispose()
	{
	}
	Transport::shared_pointer getSearchTransport()
	{
		return Transport::shared_pointer();
	}
	std::tr1::shared_ptr<Channel> getChannel(pvAccessID channelID)
	{
		return std::tr1::shared_ptr<Channel>();
	}
	Configuration::shared_pointer getConfiguration()
	{
		return Configuration::shared_pointer();
	}
	std::tr1::shared_ptr<TransportRegistry> getTransportRegistry()
	{
		return std::tr1::shared_ptr<TransportRegistry>();
	}
	void beaconAnomalyNotify() {};
private:
	Timer::shared_pointer _timer;
	void loadConfiguration() { }
	void internalInitialize() { }
	void initializeUDPTransport() { }
	void internalDestroy() { }
	void destroyAllChannels() { }
	void checkChannelName(String& name) {}
	void checkState() {	}
	pvAccessID generateCID()
	{
		return 0;
	}
	void freeCID(int cid)
	{
	}
	Transport* getTransport(TransportClient* client, osiSockAddr* serverAddress, int minorRevision, int priority)
    {
		return NULL;
   	}
	Channel* createChannelInternal(String name, ChannelRequester* requester, short priority,
			InetAddrVector* addresses)
	{
		return NULL;
	}
	void destroyChannel(ChannelImpl* channel, bool force) {
	}
	ChannelSearchManager* getChannelSearchManager() {
		return NULL;
	}

	virtual void acquire() {}
	virtual void release() {}
};

class TestSearcInstance : public BaseSearchInstance
{
public:
	TestSearcInstance(string channelName, pvAccessID channelID): _channelID(channelID), _channelName(channelName) {}
	pvAccessID getSearchInstanceID() { return _channelID;};
	string getSearchInstanceName() {return _channelName;};
	void searchResponse(int8 minorRevision, osiSockAddr* serverAddress) {};
	void acquire() {};
	void release() {};
private:
	pvAccessID _channelID;
	string _channelName;
};

static const int max_channels = 100;
ContextImpl* context = new ContextImpl();
ChannelSearchManager* manager = new ChannelSearchManager(static_cast<Context*>(context));
TestSearcInstance** chanArray = new TestSearcInstance*[max_channels];

void* testWorker1(void* p)
		{
	for(int i = 0; i < 1000; i++)
	{
		for(int j = 0; j < max_channels/2; j++)
		{
			manager->unregisterChannel(chanArray[j]);
			usleep(100);
			manager->registerChannel(chanArray[j]);
		}
	}

	return NULL;
		}


void* testWorker2(void* p)
		{
	for(int i = 0; i < 1000; i++)
	{
		for(int j = max_channels/2; j < max_channels; j++)
		{
			manager->unregisterChannel(chanArray[j]);
			usleep(100);
			manager->registerChannel(chanArray[j]);
			manager->beaconAnomalyNotify();
		}
	}

	return NULL;
		}

int main(int argc,char *argv[])
{
	pthread_t _worker1Id;
	pthread_t _worker2Id;

	ostringstream obuffer;
	for(int i = 0; i < max_channels; i++)
	{
		obuffer.clear();
		obuffer.str("");
		obuffer << i;
		string name = "chan" + obuffer.str();
		chanArray[i] = new TestSearcInstance(name.c_str(), i);
		manager->registerChannel(chanArray[i]);
	}

	//create two threads
	int32 retval = pthread_create(&_worker1Id, NULL, testWorker1, NULL);
	if(retval != 0)
	{
		assert(true);
	}

	retval = pthread_create(&_worker2Id, NULL, testWorker2, NULL);
	if(retval != 0)
	{
		assert(true);
	}

	retval = pthread_join(_worker1Id, NULL);
	if(retval != 0)
	{
		assert(true);
	}

	retval = pthread_join(_worker2Id, NULL);
	if(retval != 0)
	{
		assert(true);
	}

	manager->cancel();


	context->destroy();
	epicsExitCallAtExits();
	CDRMonitor::get().show(stdout);

	for(int i = 0; i < max_channels; i++)
	{
		if(chanArray[i]) delete chanArray[i];
	}
	if(chanArray) delete [] chanArray;
	if(manager) delete manager;
	if(context) delete context;
	return(0);
}
