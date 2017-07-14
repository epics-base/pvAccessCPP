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
struct GetWait : pvac::ClientChannel::GetCallback
{
    epicsMutex mutex;
    epicsEvent event;
    bool done;
    pvac::GetEvent result;

    GetWait() :done(false) {}
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

}//namespace pvac
