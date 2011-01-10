/* testChannelSearcManager.cpp */

#include <channelSearchManager.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

class TestSearcInstance : public BaseSearchInstance
{
public:
	TestSearcInstance(string channelName, pvAccessID channelID): _channelID(channelID), _channelName(channelName) {}
	pvAccessID getSearchInstanceID() { return _channelID;};
	string getSearchInstanceName() {return _channelName;};
	void searchResponse(int8 minorRevision, osiSockAddr* serverAddress) {};
private:
	pvAccessID _channelID;
	string _channelName;
};

int main(int argc,char *argv[])
{
    //ClientContextImpl* context = new ClientContextImpl();
    Context* context = 0;   // TODO will crash...
    ChannelSearchManager* manager = new ChannelSearchManager(context);

    TestSearcInstance* chan1 = new TestSearcInstance("chan1", 1);
    manager->registerChannel(chan1);

    sleep(3);

    manager->cancel();

    //context->destroy();
    getShowConstructDestruct()->constuctDestructTotals(stdout);

    //if(chan1) delete chan1;
    if(manager) delete manager;
    if(context) delete context;
    return(0);
}
