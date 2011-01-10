/* testChannelSearcManager.cpp */

#include <channelSearchManager.h>
#include <sstream>

using namespace epics::pvData;
using namespace epics::pvAccess;

class TestSearcInstance : public BaseSearchInstance
{
public:
	TestSearcInstance(string channelName, pvAccessID channelID): _channelID(channelID), _channelName(channelName) {}
	pvAccessID getChannelID() { return _channelID;};
	string getChannelName() {return _channelName;};
	void searchResponse(int8 minorRevision, osiSockAddr* serverAddress) {};
private:
	pvAccessID _channelID;
	string _channelName;
};

static const int max_channels = 100;
ClientContextImpl* context = new ClientContextImpl();
ChannelSearchManager* manager = new ChannelSearchManager(context);
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
    getShowConstructDestruct()->constuctDestructTotals(stdout);

    for(int i = 0; i < max_channels; i++)
    {
    	if(chanArray[i]) delete chanArray[i];
    }
    if(chanArray) delete [] chanArray;
    if(manager) delete manager;
    if(context) delete context;
    return(0);
}
