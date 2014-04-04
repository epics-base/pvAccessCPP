/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/pvData.h>
#include <pv/rpcServer.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

static StructureConstPtr resultStructure = 
    getFieldCreate()->createFieldBuilder()->
                    add("c", pvDouble)->
                    createStructure();

class SumServiceImpl :
    public RPCService
{
    PVStructure::shared_pointer request(PVStructure::shared_pointer const & args)
        throw (RPCRequestException)
    {
        PVString::shared_pointer fa = args->getSubField<PVString>("a");
        PVString::shared_pointer fb = args->getSubField<PVString>("b");
        if (!fa || !fb)
            throw RPCRequestException(Status::STATUSTYPE_ERROR, "'string a' and 'string b' fields required"); 

        double a = atof(fa->get().c_str());
        double b = atof(fb->get().c_str());

        PVStructure::shared_pointer result = getPVDataCreate()->createPVStructure(resultStructure);
        result->getSubField<PVDouble>("c")->put(a+b);
        return result;
    }
};

int main()
{
    RPCServer server;

    server.registerService("sum", RPCService::shared_pointer(new SumServiceImpl()));
    // you can register as many services as you want here ...

    server.printInfo();
    server.run();

    return 0;
}