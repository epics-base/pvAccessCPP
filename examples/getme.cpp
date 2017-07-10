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

//! [Headers]
#include <pv/configuration.h>
#include <pv/clientFactory.h>
#include <pv/caProvider.h>
#include <pv/pvaTestClient.h>
//! [Headers]

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

struct Getter : public TestClientChannel::GetCallback,
                public TestClientChannel::ConnectCallback
{
    POINTER_DEFINITIONS(Getter);

    const std::string name;
    TestClientChannel channel;
    TestOperation op;

    Getter(TestClientProvider& provider, const std::string& name)
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

    virtual void getDone(const TestGetEvent& event)
    {
        switch(event.event) {
        case TestGetEvent::Fail:
            std::cout<<"Error "<<name<<" : "<<event.message<<"\n";
            break;
        case TestGetEvent::Cancel:
            std::cout<<"Cancel "<<name<<"\n";
            break;
        case TestGetEvent::Success:
            pvd::PVField::const_shared_pointer valfld(event.value->getSubField("value"));
            if(!valfld)
                valfld = event.value;
            std::cout<<name<<" : "<<*valfld<<"\n";
            break;
        }
    }

    virtual void connectEvent(const TestConnectEvent& evt)
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
                        std::cerr << "--provider requires value\n";
                        return 1;
                    }
                } else if(strcmp(argv[i], "-T")==0 || strcmp(argv[i], "--timeout")==0) {
                    if(i<argc-1) {
                        waitTime = pvd::castUnsafe<double, std::string>(argv[++i]);
                    } else {
                        std::cerr << "--timeout requires value\n";
                        return 1;
                    }
                } else {
                    std::cerr<<"Unknown argument: "<<argv[i]<<"\n";
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
        TestClientProvider provider(providerName, conf);

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

    } catch(std::exception& e){
        std::cerr<<"Error: "<<e.what()<<"\n";
        return 1;
    }
    return 0;
}
