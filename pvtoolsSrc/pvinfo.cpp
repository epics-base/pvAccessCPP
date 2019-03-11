/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
#include <iostream>
#include <pva/client.h>
#include <pv/caProvider.h>

#include <stdio.h>
#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsThread.h>
#include <pv/logger.h>
#include <pv/lock.h>

#include <vector>
#include <string>
#include <sstream>

#include <pv/event.h>
#include <epicsExit.h>

#include "pvutils.h"

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

namespace {

void usage (void)
{
    fprintf (stderr, "\nUsage: pvinfo [options] <PV name>...\n\n"
             "\noptions:\n"
             "  -h: Help: Print this message\n"
             "  -V: Print version and exit\n"
             "  -w <sec>:          Wait time, specifies timeout, default is %f second(s)\n"
             "  -p <provider>:     Set default provider name, default is '%s'\n"
             "  -d:                Enable debug output\n"
             "  -c:                Wait for clean shutdown and report used instance count (for expert users)"
             "\nExample: pvinfo double01\n\n"
             , timeout, defaultProvider.c_str());
}

int haderror;

struct GetInfo : public pvac::ClientChannel::InfoCallback,
                 public pvac::ClientChannel::ConnectCallback,
                 public Tracker
{
    pvac::ClientChannel chan;
    pvac::Operation op;

    std::string peerName;

    explicit GetInfo(pvac::ClientChannel& chan)
        :chan(chan)
    {
        chan.addConnectListener(this);
    }
    virtual ~GetInfo()
    {
        chan.removeConnectListener(this);
    }

    virtual void connectEvent(const pvac::ConnectEvent& evt) OVERRIDE FINAL
    {
        if(evt.connected) {
            Guard G(doneLock);
            peerName = evt.peerName;
        }
    }

    virtual void infoDone(const pvac::InfoEvent& evt) OVERRIDE FINAL
    {
        std::string pname;
        {
            Guard G(doneLock);
            pname = peerName;
        }

        switch(evt.event) {
        case pvac::InfoEvent::Cancel: break;
        case pvac::InfoEvent::Fail:
            std::cerr<<op.name()<<" Error: "<<evt.message<<"\n";
            haderror = 1;
            break;
        case pvac::InfoEvent::Success: {
            std::cout<<op.name()<<"\n"
                     "Server: "<<pname<<"\n"
                     "Type:\n";
            pvd::format::indent_scope I(std::cout);
            std::cout<<evt.type<<"\n";
        }
        }
        done();
        std::cout.flush();
    }
};

} // namespace


int main (int argc, char *argv[])
{
    int opt;                    /* getopt() current option */
    bool debug = false;

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */

    while ((opt = getopt(argc, argv, ":hVw:p:dc")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'V':               /* Print version */
        {
            pva::Version version("pvinfo", "cpp",
                    EPICS_PVA_MAJOR_VERSION,
                    EPICS_PVA_MINOR_VERSION,
                    EPICS_PVA_MAINTENANCE_VERSION,
                    EPICS_PVA_DEVELOPMENT_FLAG);
            fprintf(stdout, "%s\n", version.getVersionString().c_str());
            return 0;
        }
        case 'w':               /* Set PVA timeout value */
        {
            double temp;
            if((epicsScanDouble(optarg, &temp)) != 1 || timeout <= 0.0)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('pvget -h' for help.)\n", optarg);
            } else {
                timeout = temp;
            }
        }
            break;
        case 'p':               /* Set default provider */
            defaultProvider = optarg;
            break;
        case 'd':               /* Debug log level */
            debug = true;
            break;
        case 'c':               /* Clean-up and report used instance count */
            break;
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('pvinfo -h' for help.)\n",
                    optopt);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('pvinfo -h' for help.)\n",
                    optopt);
            return 1;
        default :
            usage();
            return 1;
        }
    }

    if (argc == optind)
    {
        fprintf(stderr, "No pv name(s) specified. ('pvinfo -h' for help.)\n");
        return 1;
    }

    SET_LOG_LEVEL(debug ? pva::logLevelDebug : pva::logLevelError);

    std::vector<std::tr1::shared_ptr<GetInfo> > infos;

    pva::ca::CAClientFactory::start();

        {
        pvac::ClientProvider prov(defaultProvider);

        for(int i = optind; i<argc; i++) {
            pvac::ClientChannel chan(prov.connect(argv[i]));
            std::tr1::shared_ptr<GetInfo> info(new GetInfo(chan));
            info->op = chan.info(info.get());
            infos.push_back(info);
        }

        Tracker::prepare(); // install signal handler

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

    return haderror ? 1 : 0;
}
