/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <epicsMutex.h>
#include <epicsGuard.h>

#include <pv/current_function.h>
#include <pv/pvData.h>
#include <pv/bitSet.h>
#include <pv/reftrack.h>

#define epicsExportSharedSymbols
#include "pv/logger.h"
#include "clientpvt.h"
#include "pv/pvAccess.h"

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;
typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

namespace pvac {

struct Monitor::Impl : public pva::MonitorRequester,
                       public pvac::detail::wrapped_shared_from_this<Monitor::Impl>
{
    mutable epicsMutex mutex;
    pva::Channel::shared_pointer chan;
    operation_type::shared_pointer op;
    bool started, done, seenEmpty;

    ClientChannel::MonitorCallback *cb;
    MonitorEvent event;

    pva::MonitorElement::Ref last;

    static size_t num_instances;

    Impl(ClientChannel::MonitorCallback* cb)
        :started(false)
        ,done(false)
        ,seenEmpty(false)
        ,cb(cb)
    {REFTRACE_INCREMENT(num_instances);}
    virtual ~Impl() {REFTRACE_DECREMENT(num_instances);}

    void callEvent(Guard& G, MonitorEvent::event_t evt = MonitorEvent::Fail)
    {
        ClientChannel::MonitorCallback *cb=this->cb;
        if(!cb) return;

        event.event = evt;

        if(evt==MonitorEvent::Fail || evt==MonitorEvent::Cancel)
            this->cb = 0; // last event

        try {
            UnGuard U(G);
            cb->monitorEvent(event);
            return;
        }catch(std::exception& e){
            if(!this->cb || evt==MonitorEvent::Fail) {
                LOG(pva::logLevelError, "Unhandled exception in ClientChannel::MonitorCallback::monitorEvent(): %s", e.what());
                return;
            } else {
               event.event = MonitorEvent::Fail;
               event.message = e.what();
            }
        }
        // continues error handling
        try {
            UnGuard U(G);
            cb->monitorEvent(event);
            return;
        }catch(std::exception& e){
            LOG(pva::logLevelError, "Unhandled exception following exception in ClientChannel::MonitorCallback::monitorEvent(): %s", e.what());
        }
    }

    // called automatically via wrapped_shared_from_this
    void cancel()
    {
        operation_type::shared_pointer temp;
        {
            Guard G(mutex);

            last.reset();

            if(started && op) {
                op->stop();
                started = false;
            }
            temp.swap(op);

            callEvent(G, MonitorEvent::Cancel);
        }
        if(temp)
            temp->destroy();
    }

    virtual std::string getRequesterName() OVERRIDE FINAL
    {
        Guard G(mutex);
        return chan ? chan->getRequesterName() : "<dead>";
    }


    virtual void monitorConnect(pvd::Status const & status,
                                pva::MonitorPtr const & operation,
                                pvd::StructureConstPtr const & structure) OVERRIDE FINAL
    {
        Guard G(mutex);
        if(!cb || started || done) return;

        if(!status.isOK()) {
            event.message = status.getMessage();
        } else {
            event.message.clear();
        }
        if(!status.isSuccess()) {
            callEvent(G);

        } else {
            pvd::Status sts(operation->start());
            if(sts.isSuccess()) {
                started = true;
                /* storing raw pointer to operation, which is expected
                 * to outlive our 'op'.
                 */
                last.attach(operation);
            } else {
                event.message = sts.getMessage();
                callEvent(G);
            }
        }
    }


    virtual void channelDisconnect(bool destroy) OVERRIDE FINAL
    {
        Guard G(mutex);
        if(!cb || done) return;
        event.message = "Disconnect";
        started = false;
        callEvent(G, MonitorEvent::Disconnect);
    }

    virtual void monitorEvent(pva::MonitorPtr const & monitor) OVERRIDE FINAL
    {
        Guard G(mutex);
        if(!cb || done) return;
        event.message.clear();

        callEvent(G, MonitorEvent::Data);
    }

    virtual void unlisten(pva::MonitorPtr const & monitor) OVERRIDE FINAL
    {
        Guard G(mutex);
        if(!cb || done) return;
        done = true;

        if(seenEmpty)
            callEvent(G, MonitorEvent::Data);
        // else // wait until final poll()
    }
};

size_t Monitor::Impl::num_instances;

Monitor::Monitor(const std::tr1::shared_ptr<Impl>& impl)
    :impl(impl)
{}

Monitor::~Monitor() {}


std::string Monitor::name() const
{
    return impl ? impl->chan->getChannelName() : "<NULL>";
}

void Monitor::cancel()
{
    changed.clear();
    overrun.clear();
    root.reset();
    if(impl) impl->cancel();
}

bool Monitor::poll()
{
    if(!impl) return false;
    Guard G(impl->mutex);

    if(!impl->done && impl->last.next()) {
        root = impl->last->pvStructurePtr;
        changed = *impl->last->changedBitSet;
        overrun = *impl->last->overrunBitSet;

    } else {
        root.reset();
        changed.clear();
        overrun.clear();
    }
    return impl->seenEmpty = !!root;
}

bool Monitor::complete() const
{
    if(!impl) return true;
    Guard G(impl->mutex);
    return impl->done && impl->seenEmpty;
}

Monitor
ClientChannel::monitor(MonitorCallback *cb,
                           epics::pvData::PVStructure::const_shared_pointer pvRequest)
{
    if(!impl) throw std::logic_error("Dead Channel");
    if(!pvRequest)
        pvRequest = pvd::createRequest("field()");

    std::tr1::shared_ptr<Monitor::Impl> ret(Monitor::Impl::build(cb));
    ret->chan = getChannel();

    {
        Guard G(ret->mutex);
        ret->op = ret->chan->createMonitor(ret->internal_shared_from_this(),
                                           std::tr1::const_pointer_cast<pvd::PVStructure>(pvRequest));
    }

    return Monitor(ret);
}

namespace detail {

void registerRefTrackMonitor()
{
    epics::registerRefCounter("pvac::Monitor::Impl", &Monitor::Impl::num_instances);
}

}

}//namespace pvac
