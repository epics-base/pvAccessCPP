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
#include <pv/epicsException.h>

#define epicsExportSharedSymbols
#include "pv/logger.h"
#include "pva/client.h"
#include "pv/pvAccess.h"

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;
typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

namespace {
struct WaitCommon
{
    epicsMutex mutex;
    epicsEvent event;
    bool done;

    WaitCommon() :done(false) {}
    void wait(double timeout)
    {
        Guard G(mutex);
        while(!done) {
            UnGuard U(G);
            if(!event.wait(timeout)) {
                throw pvac::Timeout();
            }
        }
    }
};

struct GetWait : public pvac::ClientChannel::GetCallback,
                 public WaitCommon
{
    pvac::GetEvent result;

    GetWait() {}
    virtual ~GetWait() {}
    virtual void getDone(const pvac::GetEvent& evt) OVERRIDE FINAL
    {
        {
            Guard G(mutex);
            if(done) {
                LOG(pva::logLevelWarn, "oops, double event to GetCallback");
            } else {
                result = evt;
                done = true;
            }
        }
        event.signal();
    }
};
} //namespace

namespace pvac {

pvd::PVStructure::const_shared_pointer
pvac::ClientChannel::get(double timeout,
                       pvd::PVStructure::const_shared_pointer pvRequest)
{
    GetWait waiter;
    {
        Operation op(get(&waiter, pvRequest));
        waiter.wait(timeout);
    }
    switch(waiter.result.event) {
    case GetEvent::Success:
        return waiter.result.value;
    case GetEvent::Fail:
        throw std::runtime_error(waiter.result.message);
    default:
    case GetEvent::Cancel: // cancel implies timeout, which should already be thrown
        THROW_EXCEPTION2(std::logic_error, "Cancelled!?!?");
    }
}

pvd::PVStructure::const_shared_pointer
pvac::ClientChannel::rpc(double timeout,
                       const epics::pvData::PVStructure::const_shared_pointer& arguments,
                       epics::pvData::PVStructure::const_shared_pointer pvRequest)
{
    GetWait waiter;
    Operation op(rpc(&waiter, arguments, pvRequest));
    {
        Guard G(waiter.mutex);
        while(!waiter.done) {
            UnGuard U(G);
            if(!waiter.event.wait(timeout)) {
                op.cancel();
                throw Timeout();
            }
        }
    }
    if(waiter.result.event==pvac::GetEvent::Success)
        return waiter.result.value;
    else
        throw std::runtime_error(waiter.result.message);
}

namespace {
struct PutValCommon : public pvac::ClientChannel::PutCallback,
                      public WaitCommon
{
    pvac::PutEvent result;

    PutValCommon() {}
    virtual ~PutValCommon() {}

    virtual void putDone(const PutEvent& evt) OVERRIDE FINAL
    {
        {
            Guard G(mutex);
            if(done) {
                LOG(pva::logLevelWarn, "oops, double event to PutCallback");
            } else {
                result = evt;
                done = true;
            }
        }
        event.signal();
    }
};

} //namespace

namespace detail {

struct PutBuilder::Exec : public pvac::ClientChannel::PutCallback,
                          public WaitCommon
{
    detail::PutBuilder& builder;
    pvac::PutEvent result;

    Exec(detail::PutBuilder& builder)
        :builder(builder)
    {}
    virtual ~Exec() {}

    virtual void putBuild(const epics::pvData::StructureConstPtr& build, Args& args) OVERRIDE FINAL
    {
        pvd::PVDataCreatePtr create(pvd::getPVDataCreate());
        pvd::PVStructurePtr root(create->createPVStructure(build));

        for(PutBuilder::scalars_t::const_iterator it = builder.scalars.begin(), end = builder.scalars.end();
            it!=end; ++it)
        {
            if(it->value.empty())
                continue;

            pvd::PVFieldPtr fld(root->getSubField(it->name));
            if(!fld && it->required)
                throw std::runtime_error(std::string("Server does not have required field ")+it->name);
            else if(!fld)
                continue; // !it->required

            const pvd::FieldConstPtr& ftype(fld->getField());
            if(ftype->getType()==pvd::union_) {
                const pvd::Union *utype = static_cast<const pvd::Union*>(ftype.get());
                pvd::PVUnion *ufld = static_cast<pvd::PVUnion*>(fld.get());

                if(utype->isVariant()) {
                    pvd::PVScalarPtr scalar(create->createPVScalar(it->value.type()));

                    scalar->putFrom(it->value);
                    ufld->set(scalar);

                } else {
                    // attempt automagic assignment to descriminating union
                    pvd::int32 idx = utype->guess(pvd::scalar, it->value.type());

                    if(idx==-1)
                        throw std::runtime_error(std::string("Unable to descriminate union field ")+it->name);

                    ufld->select<pvd::PVScalar>(idx)->putFrom(it->value);
                }

            } else if(ftype->getType()==pvd::scalar) {
                static_cast<pvd::PVScalar*>(fld.get())->putFrom(it->value);

            } else {
                throw std::runtime_error(std::string("Type mis-match assigning scalar to field ")+it->name);

            }

            args.tosend.set(fld->getFieldOffset());
        }

        for(PutBuilder::arrays_t::const_iterator it = builder.arrays.begin(), end = builder.arrays.end();
            it!=end; ++it)
        {
            if(it->value.empty())
                continue;

            pvd::PVFieldPtr fld(root->getSubField(it->name));
            if(!fld && it->required)
                throw std::runtime_error(std::string("Server does not have required field ")+it->name);
            else if(!fld)
                continue; // !it->required

            const pvd::FieldConstPtr& ftype(fld->getField());
            if(ftype->getType()==pvd::union_) {
                const pvd::Union *utype = static_cast<const pvd::Union*>(ftype.get());
                pvd::PVUnion *ufld = static_cast<pvd::PVUnion*>(fld.get());

                if(utype->isVariant()) {
                    pvd::PVScalarArrayPtr scalar(create->createPVScalarArray(it->value.original_type()));

                    scalar->putFrom(it->value);
                    ufld->set(scalar);

                } else {
                    // attempt automagic assignment to descriminating union
                    pvd::int32 idx = utype->guess(pvd::scalarArray, it->value.original_type());

                    if(idx==-1)
                        throw std::runtime_error(std::string("Unable to descriminate union field ")+it->name);

                    ufld->select<pvd::PVScalarArray>(idx)->putFrom(it->value);
                }

            } else if(ftype->getType()==pvd::scalarArray) {
                static_cast<pvd::PVScalarArray*>(fld.get())->putFrom(it->value);

                // TODO
            } else {
                throw std::runtime_error(std::string("Type mis-match assigning scalar to field ")+it->name);

            }

            args.tosend.set(fld->getFieldOffset());
        }

        args.root = root;
    }

    virtual void putDone(const PutEvent& evt) OVERRIDE FINAL
    {
        {
            Guard G(mutex);
            if(done) {
                LOG(pva::logLevelWarn, "oops, double event to PutCallback");
            } else {
                result = evt;
                done = true;
            }
        }
        event.signal();
    }
};

void PutBuilder::exec(double timeout)
{
    Exec work(*this);
    {
        Operation op(channel.put(&work, request));
        work.wait(timeout);
    }
    switch(work.result.event) {
    case PutEvent::Success: return;
    case PutEvent::Fail:
        throw std::runtime_error(work.result.message);
    case PutEvent::Cancel:
        THROW_EXCEPTION2(std::logic_error, "Cancelled!?!");
    }
}

} // namespace detail

struct MonitorSync::SImpl : public ClientChannel::MonitorCallback
{
    const bool ourevent;
    epicsEvent * const event;

    epicsMutex mutex;
    bool hadevent;

    MonitorEvent last;

    // maintained to ensure we (MonitorCallback) outlive the subscription
    Monitor sub;

    SImpl(epicsEvent *event)
        :ourevent(!event)
        ,event(ourevent ? new epicsEvent : event)
        ,hadevent(false)
    {}
    virtual ~SImpl()
    {
        sub.cancel();
        if(ourevent)
            delete event;
    }

    virtual void monitorEvent(const MonitorEvent& evt) OVERRIDE FINAL
    {
        {
            Guard G(mutex);
            last = evt;
            hadevent = true;
        }
        event->signal();
    }
};

MonitorSync::MonitorSync(const Monitor& mon, const std::tr1::shared_ptr<SImpl>& simpl)
    :Monitor(mon.impl)
    ,simpl(simpl)
{
    simpl->sub = mon;
    event.event = MonitorEvent::Fail;
}

MonitorSync::~MonitorSync() {
}

bool MonitorSync::test()
{
    if(!simpl) throw std::logic_error("No subscription");
    Guard G(simpl->mutex);
    event = simpl->last;
    simpl->last.event = MonitorEvent::Fail;
    bool ret = simpl->hadevent;
    simpl->hadevent = false;
    return ret;
}

bool MonitorSync::wait()
{
    if(!simpl) throw std::logic_error("No subscription");
    simpl->event->wait();
    Guard G(simpl->mutex);
    event = simpl->last;
    simpl->last.event = MonitorEvent::Fail;
    bool ret = simpl->hadevent;
    simpl->hadevent = false;
    return ret;
}

bool MonitorSync::wait(double timeout)
{
    if(!simpl) throw std::logic_error("No subscription");
    bool ret = simpl->event->wait(timeout);
    if(ret) {
        Guard G(simpl->mutex);
        event = simpl->last;
        simpl->last.event = MonitorEvent::Fail;
        ret = simpl->hadevent;
        simpl->hadevent = false;
    }
    return ret;
}

void MonitorSync::wake() {
    if(simpl) simpl->event->signal();
}

MonitorSync
ClientChannel::monitor(const epics::pvData::PVStructure::const_shared_pointer &pvRequest,
                       epicsEvent *event)
{
    std::tr1::shared_ptr<MonitorSync::SImpl> simpl(new MonitorSync::SImpl(event));
    Monitor mon(monitor(simpl.get(), pvRequest));
    return MonitorSync(mon, simpl);
}

namespace {


struct InfoWait : public pvac::ClientChannel::InfoCallback,
                 public WaitCommon
{
    pvac::InfoEvent result;

    InfoWait() {}
    virtual ~InfoWait() {}
    virtual void infoDone(const pvac::InfoEvent& evt) OVERRIDE FINAL
    {
        {
            Guard G(mutex);
            if(done) {
                LOG(pva::logLevelWarn, "oops, double event to InfoCallback");
            } else {
                result = evt;
                done = true;
            }
        }
        event.signal();
    }
};

} // namespace

epics::pvData::FieldConstPtr
ClientChannel::info(double timeout, const std::string& subfld)
{
    InfoWait waiter;
    {
        Operation op(info(&waiter, subfld));
        waiter.wait(timeout);
    }
    switch(waiter.result.event) {
    case InfoEvent::Success:
        return waiter.result.type;
    case InfoEvent::Fail:
        throw std::runtime_error(waiter.result.message);
    default:
    case InfoEvent::Cancel: // cancel implies timeout, which should already be thrown
        THROW_EXCEPTION2(std::logic_error, "Cancelled!?!?");
    }
}


}//namespace pvac
