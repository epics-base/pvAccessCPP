/*
 * testServerContext.cpp
 */

#include "serverContext.h"
#include <CDRMonitor.h>
#include <epicsExit.h>

using namespace epics::pvAccess;
using namespace epics::pvData;
using namespace std;

void testServerContext()
{

	ServerContextImpl ctx;

	ctx.initialize(NULL);

	ctx.printInfo();

	ctx.run(1);

	ctx.destroy();
}

int main(int argc, char *argv[])
{
	testServerContext();

	cout << "Done" << endl;

	epicsExitCallAtExits();
    CDRMonitor::get().show(stdout);
    return (0);
}
