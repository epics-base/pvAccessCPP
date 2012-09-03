/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/convert.h>
#include <pv/rpcServer.h>

using namespace epics::pvData;
using namespace epics::pvAccess;



static StructureConstPtr createResultField()
{
    FieldCreatePtr fieldCreate = getFieldCreate();
    
    StringArray fieldNames;
    fieldNames.push_back("c");
    FieldConstPtrArray fields;
    fields.push_back(fieldCreate->createScalar(pvDouble));
    return fieldCreate->createStructure(fieldNames, fields);
}

static StructureConstPtr resultStructure = createResultField();

class SumServiceImpl :
    public RPCService
{
    PVStructure::shared_pointer request(PVStructure::shared_pointer const & args)
        throw (RPCRequestException)
    {
        // TODO error handling
        double a = atof(args->getStringField("a")->get().c_str());
        double b = atof(args->getStringField("b")->get().c_str());

        PVStructure::shared_pointer result = getPVDataCreate()->createPVStructure(resultStructure);
        result->getDoubleField("c")->put(a+b);
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