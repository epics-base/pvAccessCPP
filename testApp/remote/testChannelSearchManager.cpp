/* testChannelSearcManager.cpp */

#include <channelSearchManager.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

int main(int argc,char *argv[])
{
    Context* context = 0;   // TODO will crash...
    ChannelSearchManager* manager = new ChannelSearchManager(context);

   // context->destroy();
    getShowConstructDestruct()->constuctDestructTotals(stdout);
    return(0);
}
