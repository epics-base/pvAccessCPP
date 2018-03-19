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
#include <epicsGetopt.h>

//! [Headers]
#include <pv/configuration.h>
#include <pv/caProvider.h>
#include <pva/client.h>
//! [Headers]
#include <pv/epicsException.h>

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

namespace {

typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

epicsMutex mutex;
epicsEvent done;
size_t waitingFor;

#ifdef USE_SIGNAL
void alldone(int num)
{
    (void)num;
    done.signal();
}
#endif

struct PutTracker : public pvac::ClientChannel::PutCallback
{
    POINTER_DEFINITIONS(PutTracker);

    pvac::Operation op;
    const std::string value;

    PutTracker(pvac::ClientChannel& channel,
               const pvd::PVStructure::const_shared_pointer& pvReq,
               const std::string& value)
        :op(channel.put(this, pvReq)) // put() starts here
        ,value(value)
    {}

    virtual ~PutTracker()
    {
        op.cancel();
    }

    virtual void putBuild(const epics::pvData::StructureConstPtr &build, pvac::ClientChannel::PutCallback::Args& args) OVERRIDE FINAL
    {
        // At this point we have the user provided value string 'value'
        // and the server provided structure (with types).

        // note: an exception thrown here will result in putDone() w/ Fail

        // allocate a new structure instance.
        // we are one-shot so don't bother to re-use
        pvd::PVStructurePtr root(pvd::getPVDataCreate()->createPVStructure(build));

        // we only know about writes to scalar 'value' field
        pvd::PVScalarPtr valfld(root->getSubFieldT<pvd::PVScalar>("value"));

        // attempt convert string to actual field type
        valfld->putFrom(value);

        args.root = root; // non-const -> const

        // mark only 'value' field to be sent.
        // other fields w/ default values won't be sent.
        args.tosend.set(valfld->getFieldOffset());

        std::cout<<"Put value "<<valfld<<" sending="<<args.tosend<<"\n";
    }

    virtual void putDone(const pvac::PutEvent &evt) OVERRIDE FINAL
    {
        switch(evt.event) {
        case pvac::PutEvent::Fail:
            std::cerr<<op.name()<<" Error: "<<evt.message<<"\n";
            break;
        case pvac::PutEvent::Cancel:
            std::cerr<<op.name()<<" Cancelled\n";
            break;
        case pvac::PutEvent::Success:
            std::cout<<op.name()<<" Done\n";
        }
        {
            Guard G(mutex);
            waitingFor--;
        }
        done.signal();
    }
};

void usage()
{
    std::cout<<"Usage: putme [-h] [-P <provider>] [-w <timeout>] [-r <request>] pvname=value ...\n";
}

std::string strip(const std::string& inp)
{
    size_t f=inp.find_first_not_of(" \t\n\r"),
           l=inp.find_last_not_of (" \t\n\r");
    if(f==inp.npos || f>l)
        throw std::invalid_argument("Empty string");
    return inp.substr(f, l-f+1);
}

} // namespace

int main(int argc, char *argv[]) {
    try {
        double waitTime = 5.0;
        std::string providerName("pva"), request("field()");

        int opt;
        while( (opt=getopt(argc, argv, "hP:w:r:"))!=-1)
        {
            switch(opt) {
            case 'P':
                providerName = optarg;
                break;
            case 'w':
                waitTime = pvd::castUnsafe<double, std::string>(optarg);
                break;
            case 'r':
                request = optarg;
                break;
            default:
                std::cerr<<"Unknown argument "<<opt<<"\n";
                /* fall through */
            case 'h':
                usage();
                return 1;
            }
        }

        typedef std::vector<std::pair<std::string, std::string> > args_t;
        args_t args;

        for(int i=optind; i<argc; i++)
        {
            std::string arg(argv[i]);
            size_t eq = arg.find('=');

            if(eq==arg.npos) {
                std::cerr<<"Missing '=' in \""<<arg<<"\"\n";
                usage();
                return 1;
            }

            std::string pv (strip(arg.substr(0, eq))),
                        val(strip(arg.substr(eq+1)));
            args.push_back(std::make_pair(pv, val));
        }

        // build "pvRequest" which asks for all fields
        pvd::PVStructure::const_shared_pointer pvReq(pvd::createRequest(request));

        // explicitly select configuration from process environment
        pva::Configuration::shared_pointer conf(pva::ConfigurationBuilder()
                                                .push_env()
                                                .build());

        // "pva" provider automatically in registry
        // add "ca" provider to registry
        pva::ca::CAClientFactory::start();

        std::cout<<"Use provider: "<<providerName<<"\n";
        pvac::ClientProvider provider(providerName, conf);

        std::vector<PutTracker::shared_pointer> ops(args.size());

        {
            Guard G(mutex);
            waitingFor = args.size();
        }

        for(size_t i=0; i<args.size(); i++)
        {
            args_t::const_reference arg = args[i];

            pvac::ClientChannel chan(provider.connect(arg.first));

            PutTracker::shared_pointer op(new PutTracker(chan, pvReq, arg.second));

            ops.push_back(op);
        }

#ifdef USE_SIGNAL
        signal(SIGINT, alldone);
        signal(SIGTERM, alldone);
        signal(SIGQUIT, alldone);
#endif

        int ret = 0;
        {
            Guard G(mutex);
            while(waitingFor) {
                UnGuard U(G);
                if(waitTime<0.0) {
                    done.wait();
                } else if(!done.wait(waitTime)) {
                    std::cerr<<"Timeout\n";
                    ret = 1;
                    break; // timeout
                }
            }
        }


        return ret;
    } catch(std::exception& e){
        PRINT_EXCEPTION(e);
        std::cerr<<"Error: "<<e.what()<<"\n";
        return 2;
    }
}
