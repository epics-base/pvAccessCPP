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

// s1 starts with s2 check
static bool starts_with(const std::string& s1, const std::string& s2) {
    return s2.size() <= s1.size() && s1.compare(0, s2.size(), s2) == 0;
}

class SumServiceImpl :
    public RPCService
{
    PVStructure::shared_pointer request(PVStructure::shared_pointer const & pvArguments)
    {
        // NTURI support
        PVStructure::shared_pointer args(
            (starts_with(pvArguments->getStructure()->getID(), "epics:nt/NTURI:1.")) ?
            pvArguments->getSubField<PVStructure>("query") :
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
        result->getSubFieldT<PVDouble>("c")->put(a+b);
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
