/*
 * testServerContext.cpp
 */

#include "serverContext.h"

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
    return (0);
}
