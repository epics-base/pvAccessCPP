#include <epicsExit.h>
#include <pv/clientFactory.h>
#include <pv/pvAccess.h>

int main()
{
    epics::pvAccess::ClientFactory::start();
    epics::pvAccess::getChannelProviderRegistry()->getProvider("pva");
    epics::pvAccess::ClientFactory::stop();

    //epicsThreadSleep ( 3.0 );
    //epicsExitCallAtExits();

    return 0;
}
