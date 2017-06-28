
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
#include <epicsMutex.h>
#include <epicsGuard.h>

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
void sigdone(int num)
{
    (void)num;
    done.signal();
}
#endif

struct MonTracker : public epicsThreadRunable,
                    public pva::MonitorRequester,
                    public std::tr1::enable_shared_from_this<MonTracker>
{
    POINTER_DEFINITIONS(MonTracker);
    pva::Channel::shared_pointer chan;
    pva::Monitor::shared_pointer op;

    virtual std::string getRequesterName() { return "MonTracker"; }

    virtual void monitorConnect(pvd::Status const & status,
                                pva::MonitorPtr const & monitor,
                                pvd::StructureConstPtr const & structure)
    {
        if(status.isSuccess() && !this->alldone) {
            Guard G(this->mutex);

            if(!this->op) {
                // called during createMonitor()
                this->op = monitor;
            }

            // store type info
            // also serves as "connected" flag
            this->cur_type = structure;

            // use 'monitor' arg as owner->mon may not be assigned yet
            pvd::Status msts(monitor->start());
            std::cout<<"monitorConnect "<<this->chan->getChannelName()<<" start "<<msts<<"\n";
        }
    }

    virtual void channelDisconnect(bool destroy) {
        {
            Guard G(this->mutex);

            this->cur_type.reset();
            this->alldone |= destroy;

            // no need to call self->op->stop()
            // monitor implicitly stopped on disconnect
            pvd::Status msts(this->op->stop());
        }
        try {
            monwork.push(shared_from_this());
        }catch(std::exception& e){
            Guard G(this->mutex);
            this->queued = false;
            std::cout<<"channelDisconnect failed to queue "<<e.what()<<"\n";
        }
    }

    virtual void monitorEvent(pva::MonitorPtr const & monitor)
    {
        {
            Guard G(this->mutex);
            if(this->queued) return;
            this->queued = true;
        }
        try {
            monwork.push(shared_from_this());
        }catch(std::exception& e){
            Guard G(this->mutex);
            this->queued = false;
            std::cout<<"monitorEvent failed to queue "<<e.what()<<"\n";
        }
    }

    virtual void unlisten(pva::MonitorPtr const & monitor)
    {
        std::cout<<"monitor unlisten\n";
        // handled the same as destroy
        channelDisconnect(true);
    }

    epicsMutex mutex;
    pvd::StructureConstPtr cur_type;
    bool alldone;
    bool queued;

    MonTracker() :alldone(false), queued(false) {}
    virtual ~MonTracker() {}

    virtual void run()
    {
        bool disconn;
        {
            Guard G(mutex);
            queued = false;
            disconn = !cur_type;
        }
        while(true) {
            pva::MonitorElementPtr elem(op->poll());
            if(!elem) break;
            try {
                pvd::PVField::shared_pointer fld(elem->pvStructurePtr->getSubField("value"));
                if(!fld)
                    fld = elem->pvStructurePtr;
                std::cout<<"Event "<<chan->getChannelName()<<" "<<fld
                         <<" Changed:"<<*elem->changedBitSet
                         <<" overrun:"<<*elem->overrunBitSet<<"\n";
            } catch(...) {
                op->release(elem);
                throw;
            }
            op->release(elem);
        }
        if(disconn)
            std::cout<<"Disconnected\n";
    }
};

} // namespace

int main(int argc, char *argv[]) {
    try {
        double waitTime = -1.0;
        std::string providerName("pva"),
                    requestStr("field()");
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
                } else if(strcmp(argv[i], "-r")==0 || strcmp(argv[i], "--request")==0) {
                    if(i<argc-1) {
                        requestStr = argv[++i];
                    } else {
                        std::cout << "--request requires value\n";
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
        signal(SIGINT, sigdone);
        signal(SIGTERM, sigdone);
        signal(SIGQUIT, sigdone);
#endif

        // build "pvRequest" which asks for all fields
        pvd::PVStructure::shared_pointer pvReq(pvd::createRequest(requestStr));

        // explicitly select configuration from process environment
        pva::Configuration::shared_pointer conf(pva::ConfigurationBuilder()
                                                .push_env()
                                                .build());

        // add "pva" provider to registry
        pva::ClientFactory::start();
        // add "ca" provider to registry
        pva::ca::CAClientFactory::start();

        std::cout<<"Use provider: "<<providerName<<"\n";
        pva::ChannelProvider::shared_pointer provider(pva::ChannelProviderRegistry::clients()->createProvider(providerName, conf));
        if(!provider)
            throw std::logic_error("pva provider not registered");

        std::vector<MonTracker::shared_pointer> monitors;

        for(pvs_t::const_iterator it=pvs.begin(); it!=pvs.end(); ++it) {
            const std::string& pv = *it;

            MonTracker::shared_pointer mon(new MonTracker);

            pva::Channel::shared_pointer chan(provider->createChannel(pv));
            {
                Guard G(mon->mutex);
                mon->chan = chan;
            }

            pva::Monitor::shared_pointer M(chan->createMonitor(mon, pvReq));
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
