/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

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
#include <pv/clientFactory.h>
#include <pv/caProvider.h>
#include <pv/thread.h>
#include <pv/pvaTestClient.h>

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

namespace {

typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

struct Worker {
    virtual ~Worker() {}
    virtual void process(const TestMonitorEvent& event) =0;
};

// simple work queue with thread.
// moves monitor queue handling off of PVA thread(s)
struct WorkQueue : public epicsThreadRunable {
    epicsMutex mutex;
    typedef std::tr1::shared_ptr<Worker> value_type;
    typedef std::tr1::weak_ptr<Worker> weak_type;
    // work queue holds only weak_ptr
    // so jobs must be kept alive seperately
    typedef std::deque<std::pair<weak_type, TestMonitorEvent> > queue_t;
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

    void push(const weak_type& cb, const TestMonitorEvent& evt)
    {
        bool wake;
        {
            Guard G(mutex);
            if(!running) return; // silently refuse to queue during/after close()
            wake = queue.empty();
            queue.push_back(std::make_pair(cb, evt));
        }
        if(wake)
            event.signal();
    }

    virtual void run() OVERRIDE FINAL
    {
        Guard G(mutex);

        while(running) {
            if(queue.empty()) {
                UnGuard U(G);
                event.wait();
            } else {
                queue_t::value_type ent(queue.front());
                value_type cb(ent.first.lock());
                queue.pop_front();
                if(!cb) continue;

                try {
                    UnGuard U(G);
                    cb->process(ent.second);
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

struct MonTracker : public TestClientChannel::MonitorCallback,
                    public Worker,
                    public std::tr1::enable_shared_from_this<MonTracker>
{
    POINTER_DEFINITIONS(MonTracker);

    MonTracker(const std::string& name) :name(name) {}
    virtual ~MonTracker() {}

    const std::string name;
    TestMonitor mon;

    virtual void monitorEvent(const TestMonitorEvent& evt) OVERRIDE FINAL
    {
        // running on internal provider worker thread
        // minimize work here.
        // TODO: bound queue size
        monwork.push(shared_from_this(), evt);
    }

    virtual void process(const TestMonitorEvent& evt) OVERRIDE FINAL
    {
        // running on our worker thread
        switch(evt.event) {
        case TestMonitorEvent::Fail:
            std::cout<<"Error "<<name<<" "<<evt.message<<"\n";
            break;
        case TestMonitorEvent::Cancel:
            std::cout<<"Cancel "<<name<<"\n";
            break;
        case TestMonitorEvent::Disconnect:
            std::cout<<"Disconnect "<<name<<"\n";
            break;
        case TestMonitorEvent::Data:
            while(mon.poll()) {
                pvd::PVField::const_shared_pointer fld(mon.root->getSubField("value"));
                if(!fld)
                    fld = mon.root;

                std::cout<<"Event "<<name<<" "<<fld
                        <<" Changed:"<<mon.changed
                       <<" overrun:"<<mon.overrun<<"\n";
            }
            break;
        }
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
        TestClientProvider provider(providerName, conf);

        std::vector<MonTracker::shared_pointer> monitors;

        for(pvs_t::const_iterator it=pvs.begin(); it!=pvs.end(); ++it) {
            const std::string& pv = *it;

            MonTracker::shared_pointer mon(new MonTracker(pv));

            TestClientChannel chan(provider.connect(pv));

            mon->mon = chan.monitor(mon.get());

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
