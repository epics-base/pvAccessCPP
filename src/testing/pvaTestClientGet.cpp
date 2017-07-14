/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <epicsMutex.h>
#include <epicsGuard.h>
#include <epicsEvent.h>

#include <pv/current_function.h>
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

struct GetPutter : public pva::ChannelPutRequester,
                   public pvac::Operation::Impl
{
    mutable epicsMutex mutex;

    bool started;
    operation_type::shared_pointer op;

    pvac::ClientChannel::GetCallback *getcb;
    pvac::ClientChannel::PutCallback *putcb;
    pvac::GetEvent event;

    GetPutter(pvac::ClientChannel::GetCallback* cb) :started(false), getcb(cb), putcb(0) {}
    GetPutter(pvac::ClientChannel::PutCallback* cb) :started(false), getcb(0), putcb(cb) {}
    virtual ~GetPutter() {}

    void callEvent(Guard& G, pvac::GetEvent::event_t evt = pvac::GetEvent::Fail)
    {
        if(!putcb && !getcb) return;

        event.event = evt;
        if(putcb) {
            pvac::ClientChannel::PutCallback *cb=putcb;
            putcb = 0;
            UnGuard U(G);
            cb->putDone(event);
        }
        if(getcb) {
            pvac::ClientChannel::GetCallback *cb=getcb;
            getcb = 0;
            UnGuard U(G);
            cb->getDone(event);
        }
    }

    virtual std::string name() const OVERRIDE FINAL
    {
        Guard G(mutex);
        return op ? op->getChannel()->getChannelName() : "<dead>";
    }

    virtual void cancel() OVERRIDE FINAL
    {
        Guard G(mutex);
        if(started && op) op->cancel();
        callEvent(G, pvac::GetEvent::Cancel);
    }

    virtual std::string getRequesterName() OVERRIDE FINAL
    { return "GetPutter"; }

    virtual void channelPutConnect(
        const epics::pvData::Status& status,
        pva::ChannelPut::shared_pointer const & channelPut,
        epics::pvData::Structure::const_shared_pointer const & structure) OVERRIDE FINAL
    {
        Guard G(mutex);
        if(started) return;
        if(!putcb && !getcb) return;

        if(!status.isOK()) {
            event.message = status.getMessage();
        } else {
            event.message.clear();
        }
        if(!status.isSuccess()) {
            callEvent(G);

        } else if(getcb){
            channelPut->lastRequest();
            channelPut->get();
            started = true;

        } else if(putcb){
            pvac::ClientChannel::PutCallback *cb(putcb);
            pvd::BitSet::shared_pointer tosend(new pvd::BitSet);
            pvac::ClientChannel::PutCallback::Args args(*tosend);
            try {
                UnGuard U(G);
                cb->putBuild(structure, args);
                if(!args.root)
                    throw std::logic_error("No put value provided");
                else if(args.root->getStructure().get()!=structure.get())
                    throw std::logic_error("Provided put value with wrong type");
            }catch(std::exception& e){
                if(putcb) {
                    event.message = e.what();
                    callEvent(G);
                } else {
                    LOG(pva::logLevelInfo, "Lost exception in %s: %s", CURRENT_FUNCTION, e.what());
                }
            }
            // check putcb again after UnGuard
            if(putcb) {
                channelPut->lastRequest();
                channelPut->put(std::tr1::const_pointer_cast<pvd::PVStructure>(args.root), tosend);
                started = true;
            }
        }
    }

    virtual void channelDisconnect(bool destroy) OVERRIDE FINAL
    {
        Guard G(mutex);
        event.message = "Disconnect";

        callEvent(G);
    }

    virtual void putDone(
        const epics::pvData::Status& status,
        pva::ChannelPut::shared_pointer const & channelPut) OVERRIDE FINAL
    {
        Guard G(mutex);
        if(!putcb) return;

        if(!status.isOK()) {
            event.message = status.getMessage();
        } else {
            event.message.clear();
        }

        callEvent(G, status.isSuccess()? pvac::GetEvent::Success : pvac::GetEvent::Fail);
    }

    virtual void getDone(
        const epics::pvData::Status& status,
        pva::ChannelPut::shared_pointer const & channelPut,
        epics::pvData::PVStructure::shared_pointer const & pvStructure,
        epics::pvData::BitSet::shared_pointer const & bitSet) OVERRIDE FINAL
    {
        Guard G(mutex);
        if(!getcb) return;

        if(!status.isOK()) {
            event.message = status.getMessage();
        } else {
            event.message.clear();
        }
        event.value = pvStructure;
        // assume bitSet->get(0)==true as we only make one request

        callEvent(G, status.isSuccess()? pvac::GetEvent::Success : pvac::GetEvent::Fail);
    }
};

} //namespace

namespace pvac {

Operation
ClientChannel::get(ClientChannel::GetCallback* cb,
                  epics::pvData::PVStructure::const_shared_pointer pvRequest)
{
    if(!impl) throw std::logic_error("Dead Channel");
    if(!pvRequest)
        pvRequest = pvd::createRequest("field()");

    std::tr1::shared_ptr<GetPutter> ret(new GetPutter(cb));

    {
        Guard G(ret->mutex);
        ret->op = getChannel()->createChannelPut(ret, std::tr1::const_pointer_cast<pvd::PVStructure>(pvRequest));
    }

    return Operation(ret);
}

Operation
ClientChannel::put(PutCallback* cb,
                  epics::pvData::PVStructure::const_shared_pointer pvRequest)
{
    if(!impl) throw std::logic_error("Dead Channel");
    if(!pvRequest)
        pvRequest = pvd::createRequest("field()");

    std::tr1::shared_ptr<GetPutter> ret(new GetPutter(cb));

    {
        Guard G(ret->mutex);
        ret->op = getChannel()->createChannelPut(ret, std::tr1::const_pointer_cast<pvd::PVStructure>(pvRequest));
    }

    return Operation(ret);

}

}//namespace pvac
