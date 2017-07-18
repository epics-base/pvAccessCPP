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
    virtual void getDone(const pvac::GetEvent& evt)
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
    Operation op(get(&waiter, pvRequest));
    waiter.wait(timeout);
    if(waiter.result.event==pvac::GetEvent::Success)
        return waiter.result.value;
    else
        throw std::runtime_error(waiter.result.message);
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

    virtual void putDone(const PutEvent& evt)
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

struct PutValScalar : public PutValCommon
{
    const void* value;
    pvd::ScalarType vtype;

    PutValScalar(const void* value, pvd::ScalarType vtype) :value(value), vtype(vtype) {}
    virtual ~PutValScalar() {}

    virtual void putBuild(const epics::pvData::StructureConstPtr& build, Args& args)
    {
        pvd::PVStructurePtr root(pvd::getPVDataCreate()->createPVStructure(build));
        pvd::PVScalarPtr value(root->getSubField<pvd::PVScalar>("value"));
        if(value) {
            value->putFrom(this->value, vtype);
            args.tosend.set(value->getFieldOffset());
        } else {
            // TODO: handle enum
            throw std::runtime_error("PV has no scalar 'value' sub-field");
        }
        args.root = root;
    }

};

struct PutValArray : public PutValCommon
{
    pvd::shared_vector<const void> arr;

    PutValArray(const pvd::shared_vector<const void>& arr) :arr(arr) {}
    virtual ~PutValArray() {}

    virtual void putBuild(const epics::pvData::StructureConstPtr& build, Args& args)
    {
        pvd::PVStructurePtr root(pvd::getPVDataCreate()->createPVStructure(build));
        pvd::PVScalarArrayPtr value(root->getSubField<pvd::PVScalarArray>("value"));
        if(value) {
            value->putFrom(arr);
            args.tosend.set(value->getFieldOffset());
        } else {
            throw std::runtime_error("PV has no scalar array 'value' sub-field");
        }
        args.root = root;
    }

};
} //namespace

void
ClientChannel::putValue(const void* value,
                        pvd::ScalarType vtype,
                        double timeout,
                        pvd::PVStructure::const_shared_pointer pvRequest)
{
    PutValScalar waiter(value, vtype);
    Operation op(put(&waiter, pvRequest));
    waiter.wait(timeout);
    if(waiter.result.event==PutEvent::Success)
        return;
    else
        throw std::runtime_error(waiter.result.message);
}

void
ClientChannel::putValue(const epics::pvData::shared_vector<const void>& value,
                        double timeout,
                        epics::pvData::PVStructure::const_shared_pointer pvRequest)
{
    PutValArray waiter(value);
    Operation op(put(&waiter, pvRequest));
    waiter.wait(timeout);
    if(waiter.result.event==PutEvent::Success)
        return;
    else
        throw std::runtime_error(waiter.result.message);
}


}//namespace pvac
