/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/pvData.h>
#include <pv/rpcClient.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

static StructureConstPtr requestStructure =
    getFieldCreate()->createFieldBuilder()->
    add("a", pvString)->
    add("b", pvString)->
    createStructure();

#define TIMEOUT 3.0

int main()
{
    PVStructure::shared_pointer request = getPVDataCreate()->createPVStructure(requestStructure);
    request->getSubField<PVString>("a")->put("3.14");
    request->getSubField<PVString>("b")->put("2.71");

    std::cout<<"simple sync way, allows multiple RPC calls on the client instance\n";
    try
    {
        RPCClient::shared_pointer client = RPCClient::create("sum");
        PVStructure::shared_pointer result = client->request(request, TIMEOUT);
        std::cout << *result << std::endl;
    } catch (std::exception &e)
    {
        std::cout << "Error: " <<  e.what() << std::endl;
        return 1;
    }

    std::cout<<"async way, allows multiple RPC calls on the client instance\n";
    try
    {
        RPCClient::shared_pointer client = RPCClient::create("sum");
        client->issueRequest(request);
        // go get some coffee
        PVStructure::shared_pointer result = client->waitResponse(TIMEOUT);
        std::cout << *result << std::endl;
    } catch (std::exception &e)
    {
        std::cout << "Error:" << e.what() << std::endl;
        return 1;
    }

    return 0;
}
