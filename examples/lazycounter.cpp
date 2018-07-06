/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* A counter which only ticks if at least one client is connected to it.
 *
 * Also, the type changes (toggles between int and real) each time that
 * the first client connects.
 */

#include <stdio.h>

#include <string>
#include <sstream>
#include <map>

#if !defined(_WIN32)
#include <signal.h>
#define USE_SIGNAL
#endif

#include <epicsMutex.h>
#include <epicsGuard.h>
#include <epicsThread.h>
#include <epicsEvent.h>

#include <pv/timer.h>
#include <pv/pvData.h>
#include <pv/serverContext.h>
#include <pva/server.h>
#include <pva/sharedstate.h>

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

namespace {

epicsEvent done;

#ifdef USE_SIGNAL
void alldone(int num)
{
    (void)num;
    done.signal();
}
#endif

// for demonstration purposes, we will switch between two different types.

static pvd::StructureConstPtr int_type(pvd::getFieldCreate()->createFieldBuilder()
                                           ->add("value", pvd::pvULong)
                                           ->createStructure());

static pvd::StructureConstPtr flt_type(pvd::getFieldCreate()->createFieldBuilder()
                                           ->add("value", pvd::pvDouble)
                                           ->createStructure());

struct Counter : public pvas::SharedPV::Handler,
                 public pvd::TimerCallback,
                 public pva::Destroyable
{
    POINTER_DEFINITIONS(Counter);

    // our name, internally only for logging.
    // The searchable channel name is given to the StaticProvider
    const std::string name;
    const pvas::SharedPV::weak_pointer pv;
    pvd::Timer& timer_queue;

    // const after build()
    weak_pointer internal_self;

    mutable epicsMutex mutex;

    bool queued; // are we in the Timer queue?

    pvd::uint64 count;
    bool typesel;
    pvd::PVStructurePtr scratch;
    pvd::PVScalarPtr scratch_value;

    static Counter::shared_pointer build(const pvas::SharedPV::shared_pointer& pv,
                                         const std::string& name,
                                         pvd::Timer& timer_queue) {
        Counter::shared_pointer internal(new Counter(pv, name, timer_queue)),
                                external(internal.get(), pva::Destroyable::cleaner(internal));
        // we give out internal ref (to Timer)
        internal->internal_self = internal;
        // SharedPV keeps us alive.
        // destroy() is called when SharedPV is destroyed (or Handler is replace)
        pv->setHandler(external);
        return external;
    }

    Counter(const pvas::SharedPV::shared_pointer& pv, const std::string& name, pvd::Timer& timer_queue)
        :name(name)
        ,pv(pv)
        ,timer_queue(timer_queue)
        ,queued(false)
        ,count(0u)
        ,typesel(false)
    {}
    virtual ~Counter() {
        printf("%s: destroy\n", name.c_str());
    }

    virtual void destroy() OVERRIDE FINAL {

        {
            Guard G(mutex);
            if(!queued) return;
            queued = false;
        }
        printf("%s: shutdown\n", name.c_str());
        timer_queue.cancel(shared_pointer(internal_self));
    }

    // when we go from zero clients connected to more than one client connected.
    virtual void onFirstConnect(const pvas::SharedPV::shared_pointer& pv) OVERRIDE FINAL {
        {
            Guard G(mutex);
            assert(!queued);
            queued = true;
        }
        printf("%s: starting\n", name.c_str());
        // timer first expires after 1 second, then again every second.
        // so any operation (including pvinfo) will take 1 second.
        timer_queue.schedulePeriodic(shared_pointer(internal_self), 1.0, 1.0);
    }

    // timer expires
    virtual void callback() OVERRIDE FINAL {
        bool open;
        pvd::uint64 next;
        pvd::PVStructurePtr top;
        pvd::BitSet vmask;
        {
            Guard G(mutex);
            if(!queued) return;

            open = !scratch;
            if(open) {
                // first expiration after onFirstConnect()
                // select type.
                pvd::StructureConstPtr type = typesel ? int_type : flt_type;
                typesel = !typesel;

                scratch = pvd::getPVDataCreate()->createPVStructure(type);
                scratch_value = scratch->getSubFieldT<pvd::PVScalar>("value");
            }

            // store counter value
            next = count++;
            scratch_value->putFrom(next);
            vmask.set(scratch_value->getFieldOffset());

            // We will use the PVStructure when the lock is not held.
            // This is safe as it is only modified from this (Timer)
            // thread.
            top = scratch;
        }

        pvas::SharedPV::shared_pointer pv(this->pv);

        if(open) {
            // go from closed -> open.
            // provide initial value (and new type)
            printf("%s: open %llu\n", name.c_str(), (unsigned long long)next);
            pv->open(*top, vmask);
        } else {
            // post update
            printf("%s: tick %llu\n", name.c_str(), (unsigned long long)next);
            pv->post(*top, vmask);
        }
    }

    virtual void timerStopped() OVERRIDE FINAL {}

    // when we go from 1 client connected to zero clients connected.
    virtual void onLastDisconnect(const pvas::SharedPV::shared_pointer& pv) OVERRIDE FINAL {
        bool close;
        bool cancel;
        {
            Guard G(mutex);
            cancel = queued;
            queued = false;
            close = !!scratch;

            scratch.reset();
            scratch_value.reset();
        }
        // !close implies only all clients disconnect before timer expires the first time
        if(close) {
            printf("%s: close\n", name.c_str());
            pv->close();
        }
        if(cancel) {
            timer_queue.cancel(shared_pointer(internal_self));
        }
        printf("%s: stopping\n", name.c_str());
    }
};

} //namespace

int main(int argc, char *argv[])
{
    try {
        if(argc<=1) {
            fprintf(stderr, "Usage: %s <pvname> ...\n", argv[0]);
            return 1;
        }

        pvd::Timer timer_queue("counters", (pvd::ThreadPriority)epicsThreadPriorityMedium);

        pvas::StaticProvider provider("counters"); // provider name "counters" is arbitrary

        for(int i=1; i<argc; i++) {
            pvas::SharedPV::shared_pointer pv(pvas::SharedPV::buildReadOnly());
            Counter::shared_pointer cnt(Counter::build(pv, argv[i], timer_queue));
            provider.add(argv[i], pv);
            printf("Add counter '%s'\n", argv[i]);
        }

        // create and run network server
        pva::ServerContext::shared_pointer server(pva::ServerContext::create(
                                                      pva::ServerContext::Config()
                                                      // use default config from environment
                                                      .provider(provider.provider())
                                                      ));

#ifdef USE_SIGNAL
        signal(SIGINT, alldone);
        signal(SIGTERM, alldone);
        signal(SIGQUIT, alldone);
#endif
        server->printInfo();

        printf("Running with counters\n");

        done.wait();

        timer_queue.close(); // joins timer worker

    } catch(std::exception& e){
        std::cerr<<"Error: "<<e.what()<<"\n";
        return 1;
    }
    return 0;
}
