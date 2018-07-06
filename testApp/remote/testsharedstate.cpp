/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <pv/pvUnitTest.h>
#include <testMain.h>

#include <pva/client.h>
#include <pva/sharedstate.h>
#include <pv/current_function.h>
//#include <pv/pvAccess.h>

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

namespace {


const pvd::StructureConstPtr type(pvd::getFieldCreate()->createFieldBuilder()
                                  ->add("value", pvd::pvInt)
                                  ->createStructure());

void testNoClient()
{
    testDiag("==== %s ====", CURRENT_FUNCTION);

    pvas::SharedPV::shared_pointer pv(pvas::SharedPV::buildReadOnly());

    pvd::PVStructurePtr inst(pvd::getPVDataCreate()->createPVStructure(type));
    pvd::BitSet changed;

    testThrows(std::logic_error, pv->post(*inst, changed)); // not open()'d

    pv->close(); // close while closed is a no-op

    testThrows(std::logic_error, pv->build()); // not open()'d
}

void testGetMon()
{
    testDiag("==== %s ====", CURRENT_FUNCTION);

    std::tr1::shared_ptr<pvas::StaticProvider> prov(new pvas::StaticProvider("test"));
    std::tr1::shared_ptr<pvas::SharedPV> pv(pvas::SharedPV::buildReadOnly());

    prov->add("pv:name", pv);

    pv->open(type);

    pvd::PVStructurePtr inst(pvd::getPVDataCreate()->createPVStructure(type));
    pvd::BitSet changed;
    pvd::PVScalarPtr value(inst->getSubFieldT<pvd::PVScalar>("value"));
    value->putFrom<pvd::uint32>(42);
    changed.set(value->getFieldOffset());

    pv->post(*inst, changed);

    pvac::ClientProvider cli(prov->provider());

    pvac::ClientChannel chan(cli.connect("pv:name"));

    pvac::MonitorSync mon(chan.monitor());

    {
        pvd::PVStructure::const_shared_pointer R(chan.get());

        testEqual(R->getSubFieldT<pvd::PVScalar>("value")->getAs<pvd::uint32>(), 42u);
    }

    testOk1(mon.test());
    testEqual(mon.event.event, pvac::MonitorEvent::Data);

    {
        bool poll = mon.poll();
        testOk1(poll);
        if(poll) {
            testEqual(mon.root->getSubFieldT<pvd::PVScalar>("value")->getAs<pvd::uint32>(), 42u);
        } else {
            testSkip(1, "No data");
        }

    }

    testOk1(!mon.test());
    testOk1(!mon.poll());

    value->putFrom<pvd::uint32>(43);
    pv->post(*inst, changed);

    testOk1(mon.test());

    {
        bool poll = mon.poll();
        testOk1(poll);
        if(poll) {
            testEqual(mon.root->getSubFieldT<pvd::PVScalar>("value")->getAs<pvd::uint32>(), 43u);
        } else {
            testSkip(1, "No data");
        }

    }

    testOk1(!mon.test());
    testOk1(!mon.poll());
}

void testPutRPCCancel()
{
    testDiag("==== %s ====", CURRENT_FUNCTION);

    std::tr1::shared_ptr<pvas::StaticProvider> prov(new pvas::StaticProvider("test"));
    std::tr1::shared_ptr<pvas::SharedPV> pv(pvas::SharedPV::buildReadOnly());

    prov->add("pv:name", pv);

    pv->open(type);

    pvac::ClientProvider cli(prov->provider());

    pvac::ClientChannel chan(cli.connect("pv:name"));

    testThrows(std::runtime_error, chan.put()
                                        .set<pvd::uint32>("value", 44u)
                                        .exec());

    {
        pvd::PVStructurePtr inst(pvd::getPVDataCreate()->createPVStructure(type));
        testThrows(std::runtime_error, chan.rpc(1.0, inst))
    }
}

struct TestPutRPCHandler : public pvas::SharedPV::Handler
{
    virtual ~TestPutRPCHandler() {}
    virtual void onPut(const pvas::SharedPV::shared_pointer& pv, pvas::Operation& op) OVERRIDE FINAL
    {
        pv->post(op.value(), op.changed());
        op.complete();
    }
    virtual void onRPC(const pvas::SharedPV::shared_pointer& pv, pvas::Operation& op) OVERRIDE FINAL
    {
        pvd::PVStructurePtr reply(pvd::getPVDataCreate()->createPVStructure(type));
        reply->getSubFieldT<pvd::PVScalar>("value")->putFrom<pvd::uint32>(100);
        op.complete(*reply, pvd::BitSet());
    }
};

void testPutRPC()
{
    testDiag("==== %s ====", CURRENT_FUNCTION);

    std::tr1::shared_ptr<pvas::StaticProvider> prov(new pvas::StaticProvider("test"));
    std::tr1::shared_ptr<TestPutRPCHandler> handler(new TestPutRPCHandler);
    std::tr1::shared_ptr<pvas::SharedPV> pv(pvas::SharedPV::build(handler));

    prov->add("pv:name", pv);

    pv->open(type);

    pvac::ClientProvider cli(prov->provider());

    pvac::ClientChannel chan(cli.connect("pv:name"));

    {
        pvd::PVStructure::const_shared_pointer R(chan.get());

        testEqual(R->getSubFieldT<pvd::PVScalar>("value")->getAs<pvd::uint32>(), 0u);
    }

    chan.put()
        .set<pvd::uint32>("value", 44u)
        .exec();

    {
        pvd::PVStructure::const_shared_pointer R(chan.get());

        testEqual(R->getSubFieldT<pvd::PVScalar>("value")->getAs<pvd::uint32>(), 44u);
    }

    pvd::PVStructurePtr arg(pvd::getPVDataCreate()->createPVStructure(type));
    arg->getSubFieldT<pvd::PVScalar>("value")->putFrom<pvd::uint32>(50);

    pvd::PVStructure::const_shared_pointer reply(chan.rpc(1.0, arg));

    testEqual(reply->getSubFieldT<pvd::PVScalar>("value")->getAs<pvd::uint32>(), 100u);
}

} // namespace

MAIN(testsharedstate)
{
    testPlan(19);
    try {
        testNoClient();
        testGetMon();
        testPutRPCCancel();
        testPutRPC();
    }catch(std::exception& e){
        testAbort("Unexpected exception: %s", e.what());
    }
    return testDone();
}
