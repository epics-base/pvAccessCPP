/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <epicsMutex.h>
#include <epicsGuard.h>

#include <pv/pvData.h>
#include <pv/bitSet.h>

#define epicsExportSharedSymbols
#include "pv/logger.h"
#include "pv/pvaTestClient.h"
#include "pv/pvAccess.h"

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;
typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

namespace {

struct RPCer : public pva::ChannelRPCRequester,
               public TestOperation::Impl
{
    mutable epicsMutex mutex;

    bool started;
    operation_type::shared_pointer op;

    TestClientChannel::GetCallback *cb;
    TestGetEvent event;

    pvd::PVStructure::const_shared_pointer args;

    RPCer(TestClientChannel::GetCallback* cb,
          const pvd::PVStructure::const_shared_pointer& args) :started(false), cb(cb), args(args) {}
    virtual ~RPCer() {}

    void callEvent(Guard& G, TestGetEvent::event_t evt = TestGetEvent::Fail)
    {
        TestClientChannel::GetCallback *cb=this->cb;
        if(!cb) return;

        event.event = evt;

        this->cb = 0;

        try {
            UnGuard U(G);
            cb->getDone(event);
            return;
        }catch(std::exception& e){
            if(!this->cb || evt==TestGetEvent::Fail) {
                LOG(pva::logLevelError, "Unhandled exception in TestClientChannel::GetCallback::getDone(): %s", e.what());
            } else {
               event.event = TestGetEvent::Fail;
               event.message = e.what();
            }
        }
        // continues error handling
        try {
            UnGuard U(G);
            cb->getDone(event);
            return;
        }catch(std::exception& e){
            LOG(pva::logLevelError, "Unhandled exception following exception in TestClientChannel::GetCallback::monitorEvent(): %s", e.what());
        }
    }

    virtual std::string name() const OVERRIDE FINAL
    {
        Guard G(mutex);
        return op ? op->getChannel()->getChannelName() : "<dead>";
    }

    virtual void cancel()
    {
        Guard G(mutex);
        if(started && op) op->cancel();
        callEvent(G, TestGetEvent::Cancel);
    }

    virtual std::string getRequesterName() OVERRIDE FINAL
    { return "RPCer"; }

    virtual void channelRPCConnect(
        const epics::pvData::Status& status,
        pva::ChannelRPC::shared_pointer const & operation)
    {
        Guard G(mutex);
        if(!cb || started) return;

        if(!status.isOK()) {
            event.message = status.getMessage();
        } else {
            event.message.clear();
        }
        if(!status.isSuccess()) {
            callEvent(G);

        } else {
            operation->request(std::tr1::const_pointer_cast<pvd::PVStructure>(args));
            started = true;
        }
    }

    virtual void channelDisconnect(bool destroy) OVERRIDE FINAL
    {
        Guard G(mutex);
        event.message = "Disconnect";

        callEvent(G);
    }

    virtual void requestDone(
        const epics::pvData::Status& status,
        pva::ChannelRPC::shared_pointer const & operation,
        epics::pvData::PVStructure::shared_pointer const & pvResponse)
    {
        Guard G(mutex);
        if(!cb) return;

        if(!status.isOK()) {
            event.message = status.getMessage();
        } else {
            event.message.clear();
        }
        event.value = pvResponse;

        callEvent(G, status.isSuccess()? TestGetEvent::Success : TestGetEvent::Fail);
    }
};

}//namespace

TestOperation
TestClientChannel::rpc(GetCallback* cb,
                       const epics::pvData::PVStructure::const_shared_pointer& arguments,
                       epics::pvData::PVStructure::const_shared_pointer pvRequest)
{
    if(!impl) throw std::logic_error("Dead Channel");
    if(!pvRequest)
        pvRequest = pvd::createRequest("field()");

    std::tr1::shared_ptr<RPCer> ret(new RPCer(cb, arguments));

    {
        Guard G(ret->mutex);
        ret->op = getChannel()->createChannelRPC(ret, std::tr1::const_pointer_cast<pvd::PVStructure>(pvRequest));
    }

    return TestOperation(ret);
}
