
#include <set>
#include <queue>
#include <vector>
#include <string>
#include <exception>

#if !defined(_WIN32)
#include <signal.h>
#define USE_SIGNAL
#endif

#include <epicsEvent.h>

#include <pv/configuration.h>
#include <pv/pvAccess.h>
#include <pv/clientFactory.h>
#include <pv/caProvider.h>
#include <pv/thread.h>

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

namespace {

typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

// simple work queue with thread.
// moves monitor queue handling off of PVA thread(s)
struct WorkQueue : public epicsThreadRunable {
    epicsMutex mutex;
    typedef std::tr1::shared_ptr<epicsThreadRunable> value_t;
    // work queue holds only weak_ptr
    // so jobs must be kept alive seperately
    typedef std::deque<std::tr1::weak_ptr<epicsThreadRunable> > queue_t;
    queue_t queue;
    epicsEvent event;
    bool running;
    pvd::Thread worker;

    WorkQueue()
        :running(true)
        ,worker(pvd::Thread::Config()
                .name("Monitor handler")
                .autostart(true)
                .run(this))
    {}
    ~WorkQueue() {close();}

    void close()
    {
        {
            Guard G(mutex);
            running = false;
        }
        event.signal();
        worker.exitWait();
    }

    void push(const queue_t::value_type& v)
    {
        bool wake;
        {
            Guard G(mutex);
            if(!running) return; // silently refuse to queue during/after close()
            wake = queue.empty();
            queue.push_back(v);
        }
        if(wake)
            event.signal();
    }

    virtual void run()
    {
        Guard G(mutex);

        while(running) {
            if(queue.empty()) {
                UnGuard U(G);
                event.wait();
            } else {
                value_t ent(queue.front().lock());
                queue.pop_front();
                if(!ent) continue;

                try {
                    UnGuard U(G);
                    ent->run();
                }catch(std::exception& e){
                    std::cout<<"Error in monitor handler : "<<e.what()<<"\n";
                }
            }
        }
    }
};

WorkQueue monwork;
epicsEvent done;

#ifdef USE_SIGNAL
void alldone(int num)
{
    (void)num;
    done.signal();
}
#endif

struct MonTracker : public epicsThreadRunable
{
    POINTER_DEFINITIONS(MonTracker);
    pva::Channel::shared_pointer chan;
    pva::Monitor::shared_pointer op;

    struct Req : public pva::MonitorRequester
    {
        MonTracker::weak_pointer owner;

        Req(const MonTracker::weak_pointer& owner) :owner(owner) {}
        virtual ~Req() {}

        virtual std::string getRequesterName() { return "MonReq"; }

        virtual void monitorConnect(pvd::Status const & status,
                                    pva::MonitorPtr const & monitor,
                                    pvd::StructureConstPtr const & structure)
        {
            MonTracker::shared_pointer self(owner.lock());
            if(!self) return;

            std::cout<<"monitorConnect "<<self->chan->getChannelName()<<" "<<status<<"\n";

            if(status.isSuccess()) {
                Guard G(self->mutex);

                if(!self->op) {
                    // called during createMonitor()
                    self->op = monitor;
                }

                // store type info
                // also serves as "connected" flag
                self->cur_type = structure;

                // use 'monitor' arg as owner->mon may not be assigned yet
                pvd::Status msts(monitor->start());
                std::cout<<"monitorConnect "<<self->chan->getChannelName()<<" start "<<msts<<"\n";
            }
        }

        virtual void channelDisconnect(bool destroy) {
            MonTracker::shared_pointer self(owner.lock());
            if(!self) return;

            Guard G(self->mutex);
            std::cout<<"channelDisconnect "<<self->chan->getChannelName()<<"\n";

            self->cur_type.reset();
            self->alldone |= destroy;

            // no need to call self->op->stop()
            // monitor implicitly stopped on disconnect
            pvd::Status msts(self->op->stop());
        }

        virtual void monitorEvent(pva::MonitorPtr const & monitor)
        {
            MonTracker::shared_pointer self(owner.lock());
            if(!self) return;
            std::cout<<"monitorEvent "<<self->chan->getChannelName()<<"\n";
            {
                Guard G(self->mutex);
                if(self->queued) return;
                self->queued = true;
            }
            try {
                monwork.push(owner);
            }catch(std::exception& e){
                Guard G(self->mutex);
                self->queued = false;
                std::cout<<"monitorEvent failed to queue "<<e.what()<<"\n";
            }
        }

        virtual void unlisten(pva::MonitorPtr const & monitor)
        {
            std::cout<<"monitor unlisten\n";
            // handled the same as destroy
            channelDisconnect(true);
        }
    };
    std::tr1::shared_ptr<Req> req;

    epicsMutex mutex;
    pvd::StructureConstPtr cur_type;
    bool alldone;
    bool queued;

    MonTracker() :alldone(false), queued(false) {}
    virtual ~MonTracker() {}

    virtual void run()
    {
        {
            Guard G(mutex);
            queued = false;
        }
        while(true) {
            pva::MonitorElementPtr elem(op->poll());
            if(!elem) break;
            try {
                std::cout<<"Event "<<chan->getChannelName()<<"\n"<<elem->pvStructurePtr<<"\n";
            } catch(...) {
                op->release(elem);
                throw;
            }
            op->release(elem);
        }
    }
};

} // namespace

int main(int argc, char *argv[]) {
    try {
        double waitTime = -1.0;
        std::string providerName("pva");
        typedef std::vector<std::string> pvs_t;
        pvs_t pvs;

        for(int i=1; i<argc; i++) {
            if(argv[i][0]=='-') {
                if(strcmp(argv[i], "-P")==0 || strcmp(argv[i], "--provider")==0) {
                    if(i<argc-1) {
                        providerName = argv[++i];
                    } else {
                        std::cout << "--provider requires value\n";
                        return 1;
                    }
                } else if(strcmp(argv[i], "-T")==0 || strcmp(argv[i], "--timeout")==0) {
                    if(i<argc-1) {
                        waitTime = pvd::castUnsafe<double, std::string>(argv[++i]);
                    } else {
                        std::cout << "--timeout requires value\n";
                        return 1;
                    }
                } else {
                    std::cout<<"Unknown argument: "<<argv[i]<<"\n";
                }

            } else {
                pvs.push_back(argv[i]);
            }

        }

#ifdef USE_SIGNAL
        signal(SIGINT, alldone);
        signal(SIGTERM, alldone);
        signal(SIGQUIT, alldone);
#endif

        // build "pvRequest" which asks for all fields
        pvd::PVStructure::shared_pointer pvReq(pvd::createRequest("field()"));

        // explicitly select configuration from process environment
        pva::Configuration::shared_pointer conf(pva::ConfigurationBuilder()
                                                .push_env()
                                                .build());

        // add "pva" provider to registry
        pva::ClientFactory::start();
        // add "ca" provider to registry
        pva::ca::CAClientFactory::start();

        std::cout<<"Use provider: "<<providerName<<"\n";
        pva::ChannelProvider::shared_pointer provider(pva::getChannelProviderRegistry()->createProvider(providerName, conf));
        if(!provider)
            throw std::logic_error("pva provider not registered");

        std::vector<MonTracker::shared_pointer> monitors;

        for(pvs_t::const_iterator it=pvs.begin(); it!=pvs.end(); ++it) {
            const std::string& pv = *it;

            MonTracker::shared_pointer mon(new MonTracker);
            mon->req.reset(new MonTracker::Req(mon));

            pva::Channel::shared_pointer chan(provider->createChannel(pv));
            {
                Guard G(mon->mutex);
                mon->chan = chan;
            }

            pva::Monitor::shared_pointer M(chan->createMonitor(mon->req, pvd::createRequest("field()")));
            // monitorConnect may already be called
            {
                Guard G(mon->mutex);
                assert(!mon->op || mon->op==M);
                mon->op = M;
            }

            monitors.push_back(mon);
        }

        if(waitTime<0.0)
            done.wait();
        else
            done.wait(waitTime);

    } catch(std::exception& e){
        std::cout<<"Error: "<<e.what()<<"\n";
        return 1;
    }
    monwork.close();
    return 0;
}
