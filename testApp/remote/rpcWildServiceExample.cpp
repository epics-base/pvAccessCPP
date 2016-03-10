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
    add("channelName", pvString)->
    createStructure();

class WildServiceImpl :
    public RPCService
{
    PVStructure::shared_pointer request(PVStructure::shared_pointer const & pvArguments)
    {
        // requires NTURI as argument
        if (pvArguments->getStructure()->getID() != "epics:nt/NTURI:1.0")
            throw RPCRequestException(Status::STATUSTYPE_ERROR, "RPC argument must be a NTURI normative type");

        std::string channelName = pvArguments->getSubField<PVString>("path")->get();

        // create return structure and set data
        PVStructure::shared_pointer result = getPVDataCreate()->createPVStructure(resultStructure);
        result->getSubField<PVString>("channelName")->put(channelName);
        return result;
    }
};

int main()
{
    RPCServer server;

    server.registerService("wild*", RPCService::shared_pointer(new WildServiceImpl()));
    // you can register as many services as you want here ...

    server.printInfo();
    server.run();

    return 0;
}
