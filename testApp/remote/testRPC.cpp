
#include <pv/epicsException.h>
#include <pv/valueBuilder.h>

#include <pv/clientFactory.h>
#include <pv/rpcClient.h>
#include <pv/rpcServer.h>
#include <pv/rpcService.h>

#include <epicsUnitTest.h>
#include <testMain.h>

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

namespace {

pvd::StructureConstPtr reply_type(pvd::getFieldCreate()->createFieldBuilder()
                                  ->add("value", pvd::pvDouble)
                                  ->createStructure());

struct SumService : public pva::RPCService
{
    virtual epics::pvData::PVStructure::shared_pointer request(
        epics::pvData::PVStructure::shared_pointer const & args
    ) OVERRIDE FINAL
    {
        testDiag("request()");
        pvd::PVScalarPtr lhs(args->getSubField<pvd::PVScalar>("query.lhs")),
                         rhs(args->getSubField<pvd::PVScalar>("query.rhs"));
        if(!lhs || !rhs)
            throw pva::RPCRequestException("Missing query.lhs and/or query.rhs");

        double a = lhs->getAs<double>(),
               b = rhs->getAs<double>();

        testDiag("Add %f + %f", a, b);
        pvd::PVStructure::shared_pointer reply(pvd::getPVDataCreate()->createPVStructure(reply_type));
        reply->getSubFieldT<pvd::PVDouble>("value")->put(a+b);
        return reply;
    }
};

void testSum(const pva::ChannelProvider::shared_pointer& cli_prov)
{
    pva::RPCClient client("sum", pvd::createRequest("field()"), cli_prov);

    pvd::ValueBuilder args("epics:nt/NTURI:1.0");
    args.add<pvd::pvString>("scheme", "pva")
        .add<pvd::pvString>("path", "sum");

    pvd::PVStructurePtr reply;

    testDiag("Request");
    reply = client.request(args.addNested("query")
                               .add<pvd::pvDouble>("lhs", 5.0)
                               .add<pvd::pvDouble>("rhs", 3.0)
                           .endNested()
                           .buildPVStructure());

    pvd::int32 value = reply->getSubFieldT<pvd::PVScalar>("value")->getAs<pvd::int32>();
    testOk(value==8, "Reply value = %d", (unsigned)value);

    testDiag("Wait for connect (already connected)");
    testOk1(client.waitConnect());

}



struct FailService : public pva::RPCService
{
    virtual epics::pvData::PVStructure::shared_pointer request(
        epics::pvData::PVStructure::shared_pointer const & args
    ) OVERRIDE FINAL
    {
        testDiag("failing()");
        throw std::runtime_error("oops");
    }
};

void testRPCFail(const pva::ChannelProvider::shared_pointer& cli_prov)
{

    testDiag("Fail");

    pva::RPCClient client("fail", pvd::createRequest("field()"), cli_prov);

    pvd::ValueBuilder args("epics:nt/NTURI:1.0");
    args.add<pvd::pvString>("scheme", "pva")
        .add<pvd::pvString>("path", "fail");


    testDiag("Request");
    try{
        (void)client.request(args.addNested("query")
                             .add<pvd::pvDouble>("lhs", 5.0)
                             .add<pvd::pvDouble>("rhs", 3.0)
                             .endNested()
                             .buildPVStructure());
        testFail("Missing expected exception");
    }catch(pva::RPCRequestException& e){
        testPass("caught expected rpc exception: %s", e.what());
    }catch(std::exception& e){
        testFail("caught un-expected exception: %s", e.what());
    }
}

} // namespace

MAIN(testRPC)
{
    testPlan(3);
    try {
        pva::Configuration::shared_pointer conf(pva::ConfigurationBuilder()
                                                //.push_env()
                                                //.add("EPICS_PVA_DEBUG", "3")
                                                .add("EPICS_PVAS_INTF_ADDR_LIST", "127.0.0.1")
                                                .add("EPICS_PVA_ADDR_LIST", "127.0.0.1")
                                                .add("EPICS_PVA_AUTO_ADDR_LIST","0")
                                                .add("EPICS_PVA_SERVER_PORT", "0")
                                                .add("EPICS_PVA_BROADCAST_PORT", "0")
                                                .push_map()
                                                .build());

        testDiag("Server Setup");
        pva::RPCServer serv(conf);
        testDiag("TestServer on ports TCP=%u UDP=%u",
                 serv.getServer()->getServerPort(),
                 serv.getServer()->getBroadcastPort());

        {
            std::tr1::shared_ptr<pva::RPCService> service(new SumService);
            serv.registerService("sum", service);
        }
        {
            std::tr1::shared_ptr<pva::RPCService> service(new FailService);
            serv.registerService("fail", service);
        }

        testDiag("Client Setup");
        pva::ClientFactory::start();
        pva::ChannelProvider::shared_pointer cli_prov(pva::ChannelProviderRegistry::clients()->createProvider("pva",
                                                                                                              serv.getServer()->getCurrentConfig()));
        if(!cli_prov)
            testAbort("No pva provider");
        testDiag("Client Ready");

        testSum(cli_prov);
        testRPCFail(cli_prov);

    }catch(std::exception& e){
        PRINT_EXCEPTION(e);
        testAbort("Unexpected exception: %s", e.what());
    }
    return testDone();
}
