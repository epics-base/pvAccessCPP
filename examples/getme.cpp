
#include <set>
#include <vector>
#include <string>
#include <exception>

#if !defined(_WIN32)
#include <signal.h>
#define USE_SIGNAL
#endif

#include <epicsEvent.h>

//! [Headers]
#include <pv/configuration.h>
#include <pv/pvAccess.h>
#include <pv/clientFactory.h>
#include <pv/caProvider.h>
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

struct GetReq : public pva::ChannelGetRequester
{
    const std::string name;
    GetReq(const std::string& name) :name(name) {}
    virtual ~GetReq() {}

    virtual std::string getRequesterName() { return "GetReq"; }

    virtual void channelGetConnect(
        const epics::pvData::Status& status,
        pva::ChannelGet::shared_pointer const & channelGet,
        pvd::Structure::const_shared_pointer const & structure)
    {
        // Called each time get operation becomes "ready" (channel connected)
        if(status.isSuccess()) {
            std::cout<<"Get execute "<<name<<"\n";
            // can now execute the get operation
            channelGet->get();
        } else {
            std::cout<<"Oops GetConnect: "<<name<<" "<<status<<"\n";
        }
    }

    virtual void channelDisconnect(bool destroy) {
        // Called each time operation becomes no "ready" (channel disconnected)
        // same as channelStateChange() for DISCONNECTED and DESTROYED
        std::cout<<name<<"Get disconnected\n";
    }

    virtual void getDone(
        const epics::pvData::Status& status,
        pva::ChannelGet::shared_pointer const & channelGet,
        pvd::PVStructure::shared_pointer const & pvStructure,
        pvd::BitSet::shared_pointer const & bitSet)
    {
        // when execution completes
        if(status.isSuccess()) {
            pvd::PVFieldPtr valfld(pvStructure->getSubField("value"));
            if(!valfld)
                valfld = pvStructure;
            std::cout<<name<<" : "<<*valfld<<"\n";
        } else {
            std::cout<<name<<"Oops Get: "<<status<<"\n";
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
        pva::ChannelProvider::shared_pointer provider(pva::getChannelProviderRegistry()->createProvider(providerName, conf));
        if(!provider)
            throw std::logic_error("pva provider not registered");

        // need to store references to keep get (and channel) from being closed
        typedef std::set<pva::ChannelGet::shared_pointer> gets_t;
        gets_t gets;

        for(pvs_t::const_iterator it=pvs.begin(); it!=pvs.end(); ++it) {
            const std::string& pv = *it;

            pva::ChannelGetRequester::shared_pointer getreq(new GetReq(pv));

            pva::Channel::shared_pointer chan(provider->createChannel(pv));
            // if !chan then channelCreated() called with error status
            if(!chan) continue;

            // no need to wait for connection

            pva::ChannelGet::shared_pointer op(chan->createChannelGet(getreq, pvReq));
            // if !op then channelGetConnect() called with error status
            if(!op) continue;

            gets.insert(op);
            // drop our explicit Channel reference, ChannelGet holds an additional reference
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
