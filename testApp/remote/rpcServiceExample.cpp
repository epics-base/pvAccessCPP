/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/pvData.h>
#include <pv/rpcServer.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

static Structure::const_shared_pointer resultStructure =
        getFieldCreate()->createFieldBuilder()->
               add("c", pvDouble)->
               createStructure();

class SumServiceImpl :
    public RPCService
{
    PVStructure::shared_pointer request(PVStructure::shared_pointer const & pvArguments)
        throw (RPCRequestException)
    {
        // NTURI support
        PVStructure::shared_pointer args(
                    (pvArguments->getStructure()->getID() == "ev4:nt/NTURI:1.0") ?
                        pvArguments->getStructureField("query") :
                        pvArguments
                        );

        // get fields and check their existence
        PVScalar::shared_pointer af = args->getSubField<PVScalar>("a");
        PVScalar::shared_pointer bf = args->getSubField<PVScalar>("b");
        if (!af || !bf)
            throw RPCRequestException(Status::STATUSTYPE_ERROR, "scalar 'a' and 'b' fields are required");
        
        // get the numbers (and convert if neccessary)
        double a, b;
        try
        {
            a = af->getAs<double>();
            b = bf->getAs<double>();
        }
        catch (std::exception &e)
        {
            throw RPCRequestException(Status::STATUSTYPE_ERROR,
                std::string("failed to convert arguments to double: ") + e.what());
        }
        
        // create return structure and set data
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
