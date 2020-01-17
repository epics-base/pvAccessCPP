/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
#include <iostream>
#include <vector>
#include <set>
#include <deque>
#include <string>
#include <istream>
#include <fstream>
#include <sstream>

#include <stdio.h>

#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsExit.h>
#include <epicsGuard.h>

#include <pv/pvData.h>
#include <pv/logger.h>
#include <pv/lock.h>
#include <pv/event.h>
#include <pv/thread.h>
#include <pv/reftrack.h>

#include <pv/caProvider.h>
#include <pv/logger.h>
#include <pva/client.h>

#include "pvutils.h"

#ifndef EXECNAME
#  define EXECNAME "pvget"
#endif

namespace {

size_t pvnamewidth;

int haderror;

void usage (void)
{
    fprintf (stderr, "\nUsage: " EXECNAME " [options] <PV name>...\n"
             "\n"
             COMMON_OPTIONS
             " deprecated options:\n"
             "  -q, -t, -i, -n, -F: ignored\n"
             "  -f <input file>:   errors\n"
             " Output details:\n"
             "  -m -v:             Monitor in Raw mode.  Print only fields marked as changed.\n"
             "  -m -vv:            Monitor in Raw mode.  Highlight fields marked as changed, show all valid fields.\n"
             "  -m -vvv:           Monitor in Raw mode.  Highlight fields marked as changed, show all fields.\n"
             "  -vv:               Get in Raw mode.  Highlight valid fields, show all fields.\n"
             "\n"
             "example: " EXECNAME " double01\n\n"
             , request.c_str(), timeout, defaultProvider.c_str());
}

struct Getter : public pvac::ClientChannel::GetCallback, public Tracker
{
    POINTER_DEFINITIONS(Getter);

    pvac::Operation op;

    Getter(pvac::ClientChannel& channel, const pvd::PVStructurePtr& pvRequest)
    {
        op = channel.get(this, pvRequest);
    }
    virtual ~Getter() {}

    virtual void getDone(const pvac::GetEvent& event) OVERRIDE FINAL
    {
        std::cout<<std::setw(pvnamewidth)<<std::left<<op.name()<<' ';
        switch(event.event) {
        case pvac::GetEvent::Fail:
            std::cerr<<"Error "<<event.message<<"\n";
            haderror = 1;
            break;
        case pvac::GetEvent::Cancel:
            break;
        case pvac::GetEvent::Success: {
            pvd::PVStructure::Formatter fmt(event.value->stream()
                                            .format(outmode));

            if(verbosity>=2)
                fmt.highlight(*event.valid); // show all, highlight valid
            else
                fmt.show(*event.valid); // only show valid, highlight none

            std::cout<<fmt;
        }
            break;
        }
        std::cout.flush();
        done();
    }
};



struct Worker {
    virtual ~Worker() {}
    virtual void process(const pvac::MonitorEvent& event) =0;
};

// simple work queue with thread.
// moves monitor queue handling off of PVA thread(s)
struct WorkQueue : public epicsThreadRunable {
    epicsMutex mutex;
    typedef std::tr1::shared_ptr<Worker> value_type;
    typedef std::tr1::weak_ptr<Worker> weak_type;
    // work queue holds only weak_ptr
    // so jobs must be kept alive seperately
    typedef std::deque<std::pair<weak_type, pvac::MonitorEvent> > queue_t;
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

    void push(const weak_type& cb, const pvac::MonitorEvent& evt)
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


struct MonTracker : public pvac::ClientChannel::MonitorCallback,
                    public Worker,
                    public Tracker,
                    public std::tr1::enable_shared_from_this<MonTracker>
{
    POINTER_DEFINITIONS(MonTracker);

    MonTracker(WorkQueue& monwork, pvac::ClientChannel& channel, const pvd::PVStructurePtr& pvRequest)
        :monwork(monwork)
        ,mon(channel.monitor(this, pvRequest))
    {}
    virtual ~MonTracker() {mon.cancel();}

    WorkQueue& monwork;

    pvd::BitSet valid; // only access for process()

    pvac::Monitor mon; // must be last data member

    virtual void monitorEvent(const pvac::MonitorEvent& evt) OVERRIDE FINAL
    {
        // shared_from_this() will fail as Cancel is delivered in our dtor.
        if(evt.event==pvac::MonitorEvent::Cancel) return;

        // running on internal provider worker thread
        // minimize work here.
        monwork.push(shared_from_this(), evt);
    }

    virtual void process(const pvac::MonitorEvent& evt) OVERRIDE FINAL
    {
        // running on our worker thread
        switch(evt.event) {
        case pvac::MonitorEvent::Fail:
            std::cerr<<std::setw(pvnamewidth)<<std::left<<mon.name()<<" Error "<<evt.message<<"\n";
            haderror = 1;
            done();
            break;
        case pvac::MonitorEvent::Cancel:
            break;
        case pvac::MonitorEvent::Disconnect:
            std::cout<<std::setw(pvnamewidth)<<std::left<<mon.name()<<" <Disconnect>\n";
            valid.clear();
            break;
        case pvac::MonitorEvent::Data:
        {
            unsigned n;
            for(n=0; n<2 && mon.poll(); n++) {
                valid |= mon.changed;

                pvd::PVStructure::Formatter fmt(mon.root->stream()
                                                .format(outmode));

                if(verbosity>=3)
                    fmt.highlight(mon.changed); // show all
                else if(verbosity>=2)
                    fmt.highlight(mon.changed).show(valid);
                else
                    fmt.show(mon.changed); // highlight none

                std::cout<<std::setw(pvnamewidth)<<std::left<<mon.name()<<' '<<fmt;
            }
            if(n==2) {
                // too many updates, re-queue to balance with others
                monwork.push(shared_from_this(), evt);
            } else if(n==0) {
                LOG(pva::logLevelDebug, "%s Spurious Data event on channel", mon.name().c_str());
            } else {
                if(mon.complete())
                    done();
            }
        }
            break;
        }
        std::cout.flush();
    }
};

} // namespace

#ifndef MAIN
#  define MAIN main
#endif

int MAIN (int argc, char *argv[])
{
    try {
        int opt;                    /* getopt() current option */
#ifdef PVMONITOR
        bool monitor = true;
#else
        bool monitor = false;
#endif

        epics::RefMonitor refmon;

        // ================ Parse Arguments

        while ((opt = getopt(argc, argv, ":hvVRM:r:w:tmp:qdcF:f:ni")) != -1) {
            switch (opt) {
            case 'h':               /* Print usage */
                usage();
                return 0;
            case 'v':
                verbosity++;
                break;
            case 'V':               /* Print version */
            {
                fprintf(stdout, "pvAccess %u.%u.%u%s\n",
                        EPICS_PVA_MAJOR_VERSION,
                        EPICS_PVA_MINOR_VERSION,
                        EPICS_PVA_MAINTENANCE_VERSION,
                        (EPICS_PVA_DEVELOPMENT_FLAG)?"-SNAPSHOT":"");
                fprintf(stdout, "pvData %u.%u.%u%s\n",
                        EPICS_PVD_MAJOR_VERSION,
                        EPICS_PVD_MINOR_VERSION,
                        EPICS_PVD_MAINTENANCE_VERSION,
                        (EPICS_PVD_DEVELOPMENT_FLAG)?"-SNAPSHOT":"");
                fprintf(stdout, "Base %s\n", EPICS_VERSION_FULL);
                return 0;
            }
            case 'R':
                refmon.start(5.0);
                break;
            case 'M':
                if(strcmp(optarg, "raw")==0) {
                    outmode = pvd::PVStructure::Formatter::Raw;
                } else if(strcmp(optarg, "nt")==0) {
                    outmode = pvd::PVStructure::Formatter::NT;
                } else if(strcmp(optarg, "json")==0) {
                    outmode = pvd::PVStructure::Formatter::JSON;
                } else {
                    fprintf(stderr, "Unknown output mode '%s'\n", optarg);
                    outmode = pvd::PVStructure::Formatter::Raw;
                }
                break;
            case 'w':               /* Set PVA timeout value */
            {
                double temp;
                if((epicsScanDouble(optarg, &temp)) != 1)
                {
                    fprintf(stderr, "'%s' is not a valid timeout value "
                                    "- ignored. ('" EXECNAME " -h' for help.)\n", optarg);
                } else {
                    timeout = temp;
                }
            }
                break;
            case 'r':               /* Set PVA timeout value */
                request = optarg;
                break;
            case 't':               /* Terse mode */
            case 'i':               /* T-types format mode */
            case 'F':               /* Store this for output formatting */
            case 'n':
            case 'q':               /* Quiet mode */
                // deprecate
                break;
            case 'f':               /* Use input stream as input */
                fprintf(stderr, "Unsupported option -f\n");
                return 1;
            case 'm':               /* Monitor mode */
                monitor = true;
                break;
            case 'p':               /* Set default provider */
                defaultProvider = optarg;
                break;
            case 'd':               /* Debug log level */
                debugFlag = true;
                break;
            case 'c':               /* Clean-up and report used instance count */
                break;
            case '?':
                fprintf(stderr,
                        "Unrecognized option: '-%c'. ('" EXECNAME " -h' for help.)\n",
                        optopt);
                return 1;
            case ':':
                fprintf(stderr,
                        "Option '-%c' requires an argument. ('" EXECNAME " -h' for help.)\n",
                        optopt);
                return 1;
            default :
                usage();
                return 1;
            }
        }

        if(monitor)
            timeout = -1;

        if(verbosity>0 && outmode==pvd::PVStructure::Formatter::NT)
            outmode = pvd::PVStructure::Formatter::Raw;

        pvd::PVStructure::shared_pointer pvRequest;
        try {
            pvRequest = pvd::createRequest(request);
        } catch(std::exception& e){
            fprintf(stderr, "failed to parse request string: %s\n", e.what());
            return 1;
        }

        for(int i = optind; i < argc; i++) {
            pvnamewidth = std::max(pvnamewidth, strlen(argv[i]));
        }

        SET_LOG_LEVEL(debugFlag ? pva::logLevelDebug : pva::logLevelError);

        epics::pvAccess::ca::CAClientFactory::start();

        {
            pvac::ClientProvider provider(defaultProvider);

            std::vector<std::tr1::shared_ptr<Tracker> > tracked;

            epics::auto_ptr<WorkQueue> Q;
            if(monitor)
                Q.reset(new WorkQueue);

            for(int i = optind; i < argc; i++) {
                pvac::ClientChannel chan(provider.connect(argv[i]));

                if(monitor) {
                    std::tr1::shared_ptr<MonTracker> mon(new MonTracker(*Q, chan, pvRequest));

                    tracked.push_back(mon);

                } else { // Get
                    std::tr1::shared_ptr<Getter> get(new Getter(chan, pvRequest));

                    tracked.push_back(get);
                }
            }

            // ========================== Wait for operations to complete, or timeout

            Tracker::prepare(); // install signal handler

            if(debugFlag)
                std::cerr<<"Waiting...\n";

            {
                Guard G(Tracker::doneLock);
                while(Tracker::inprog.size() && !Tracker::abort) {
                    UnGuard U(G);
                    if(timeout<=0)
                        Tracker::doneEvt.wait();
                    else if(!Tracker::doneEvt.wait(timeout)) {
                        haderror = 1;
                        std::cerr<<"Timeout\n";
                        break;
                    }
                }
            }
        }

        if(refmon.running()) {
            refmon.stop();
            // show final counts
            refmon.current();
        }

        // ========================== All done now

        if(debugFlag)
            std::cerr<<"Done\n";

        return haderror ? 1 : 0;
    } catch(std::exception& e) {
        std::cerr<<"Error: "<<e.what()<<"\n";
        return 1;
    }
}
