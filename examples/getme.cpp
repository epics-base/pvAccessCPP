/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <set>
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

#include <pv/configuration.h>
#include <pv/caProvider.h>
#include <pv/reftrack.h>
#include <pva/client.h>

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

namespace {

epicsEvent done;

#ifdef USE_SIGNAL
void alldone(int num)
{
    (void)num;
    done.signal();
}
#endif

struct Getter : public pvac::ClientChannel::GetCallback,
                public pvac::ClientChannel::ConnectCallback
{
    POINTER_DEFINITIONS(Getter);

    const std::string name;
    pvac::ClientChannel channel;
    pvac::Operation op;

    Getter(pvac::ClientProvider& provider, const std::string& name)
        :name(name)
        ,channel(provider.connect(name))
    {
        channel.addConnectListener(this);
    }
    virtual ~Getter()
    {
        channel.removeConnectListener(this);
        op.cancel();
    }

    virtual void getDone(const pvac::GetEvent& event) OVERRIDE FINAL
    {
        switch(event.event) {
        case pvac::GetEvent::Fail:
            std::cout<<"Error "<<name<<" : "<<event.message<<"\n";
            break;
        case pvac::GetEvent::Cancel:
            std::cout<<"Cancel "<<name<<"\n";
            break;
        case pvac::GetEvent::Success:
            pvd::PVField::const_shared_pointer valfld(event.value->getSubField("value"));
            if(!valfld)
                valfld = event.value;
            std::cout<<name<<" : "<<*valfld<<"\n";
            break;
        }
    }

    virtual void connectEvent(const pvac::ConnectEvent& evt) OVERRIDE FINAL
    {
        if(evt.connected) {
            op = channel.get(this);
        } else {
            std::cout<<"Disconnect "<<name<<"\n";
        }
    }
};

} // namespace

int main(int argc, char *argv[]) {
    try {
        epics::RefMonitor refmon;
        double waitTime = -1.0;
        std::string providerName("pva");
        typedef std::vector<std::string> pvs_t;
        pvs_t pvs;

        int opt;
        while((opt = getopt(argc, argv, "hRp:w:")) != -1) {
            switch(opt) {
            case 'R':
                refmon.start(5.0);
                break;
            case 'p':
                providerName = optarg;
                break;
            case 'w':
                waitTime = pvd::castUnsafe<double, std::string>(optarg);
                break;
            case 'h':
                std::cout<<"Usage: "<<argv[0]<<" [-p <provider>] [-w <timeout>] [-R] <pvname> ...\n";
                return 0;
            default:
                std::cerr<<"Unknown argument: "<<(int)opt<<"\n";
                return -1;
            }
        }

        for(int i=optind; i<argc; i++)
            pvs.push_back(argv[i]);

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

        // "pva" provider automatically in registry
        // add "ca" provider to registry
        pva::ca::CAClientFactory::start();

        std::cout<<"Use provider: "<<providerName<<"\n";
        pvac::ClientProvider provider(providerName, conf);

        // need to store references to keep get (and channel) from being closed
        typedef std::set<Getter::shared_pointer> gets_t;
        gets_t gets;

        for(pvs_t::const_iterator it=pvs.begin(); it!=pvs.end(); ++it) {
            const std::string& pv = *it;

            Getter::shared_pointer get(new Getter(provider, pv));
            // addConnectListener() always invokes connectEvent() with current state

            gets.insert(get);
        }

        if(waitTime<0.0)
            done.wait();
        else
            done.wait(waitTime);

        if(refmon.running()) {
            refmon.stop();
            // drop refs to operations, but keep ref to ClientProvider
            gets.clear();
            // show final counts
            refmon.current();
        }

    } catch(std::exception& e){
        std::cerr<<"Error: "<<e.what()<<"\n";
        return 1;
    }
    return 0;
}
