/* testChannelSearcManager.cpp */

#include <epicsExit.h>
#include <epicsThread.h>
#include <epicsMessageQueue.h>
#include <pv/channelSearchManager.h>
#include <sstream>

epicsMessageQueueId join1;
epicsMessageQueueId join2;

using std::string;
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
	void checkChannelName(std::string const & name) {}
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
	Channel* createChannelInternal(std::string name, ChannelRequester* requester, short priority,
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

void testWorker1(void* p)
		{
	for(int i = 0; i < 1000; i++)
	{
		for(int j = 0; j < max_channels/2; j++)
		{
			manager->unregisterChannel(chanArray[j]);
			epicsThreadSleep(100e-6);
			manager->registerChannel(chanArray[j]);
		}
	}
        int dummy = 1;
        epicsMessageQueueSend(join1, &dummy, 1);
		}


void testWorker2(void* p)
		{
	for(int i = 0; i < 1000; i++)
	{
		for(int j = max_channels/2; j < max_channels; j++)
		{
			manager->unregisterChannel(chanArray[j]);
			epicsThreadSleep(100e-6);
			manager->registerChannel(chanArray[j]);
			manager->beaconAnomalyNotify();
		}
	}

        int dummy = 2;
        epicsMessageQueueSend(join1, &dummy, 1);
		}

int main(int argc,char *argv[])
{
	epicsThreadId _worker1Id;
	epicsThreadId _worker2Id;

	std::ostringstream obuffer;
	for(int i = 0; i < max_channels; i++)
	{
		obuffer.clear();
		obuffer.str("");
		obuffer << i;
		string name = "chan" + obuffer.str();
		chanArray[i] = new TestSearcInstance(name.c_str(), i);
		manager->registerChannel(chanArray[i]);
	}

        join1 = epicsMessageQueueCreate(1, 1);
        join2 = epicsMessageQueueCreate(1, 1);

	//create two threads
	_worker1Id = epicsThreadCreate("worker1", epicsThreadPriorityMedium, epicsThreadGetStackSize(epicsThreadStackMedium),
                                         testWorker1, NULL);
        assert(_worker1Id != NULL);

	_worker2Id = epicsThreadCreate("worker2", epicsThreadPriorityMedium, epicsThreadGetStackSize(epicsThreadStackMedium),
                                         testWorker2, NULL);
        assert(_worker1Id != NULL);

        int dummy;
        epicsMessageQueueReceive(join1, &dummy, 1);
        epicsMessageQueueReceive(join2, &dummy, 1);

	manager->cancel();


	context->destroy();
	//epicsExitCallAtExits();

	for(int i = 0; i < max_channels; i++)
	{
		if(chanArray[i]) delete chanArray[i];
	}
	if(chanArray) delete [] chanArray;
	if(manager) delete manager;
	if(context) delete context;
	return(0);
}
