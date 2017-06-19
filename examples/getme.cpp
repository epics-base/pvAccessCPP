
#include <set>
#include <exception>

#include <epicsEvent.h>

#include <pv/configuration.h>
#include <pv/pvAccess.h>
#include <pv/clientFactory.h>

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

namespace {

epicsEvent done;

struct ChanReq : public pva::ChannelRequester
{
    virtual ~ChanReq() {}

    virtual std::string getRequesterName() { return "ChanReq"; }

    virtual void channelCreated(const epics::pvData::Status& status,
                                pva::Channel::shared_pointer const & channel) {
        if(!status.isSuccess()) {
            std::cout<<"Oops Connect: "<<status<<"\n";
        } else {
            std::cout<<"Connect: "<<channel->getChannelName()<<"\n";
        }
    }

    virtual void channelStateChange(pva::Channel::shared_pointer const & channel,
                                    pva::Channel::ConnectionState connectionState) {
        switch(connectionState) {
        case pva::Channel::NEVER_CONNECTED:
        case pva::Channel::CONNECTED:
        case pva::Channel::DISCONNECTED:
        case pva::Channel::DESTROYED:
            std::cout<<channel->getChannelName()<<" "<<pva::Channel::ConnectionStateNames[connectionState]<<"\n";
            break;

        default:
            break;
        }
    }
};

struct GetReq : public pva::ChannelGetRequester
{
    virtual ~GetReq() {}

    virtual std::string getRequesterName() { return "GetReq"; }

    virtual void channelGetConnect(
        const epics::pvData::Status& status,
        pva::ChannelGet::shared_pointer const & channelGet,
        pvd::Structure::const_shared_pointer const & structure)
    {
        if(status.isSuccess()) {
            std::cout<<"Get request"<<channelGet->getChannel()->getChannelName()<<"\n";
            channelGet->get();
        } else {
            std::cout<<"Oops GetConnect: "<<channelGet->getChannel()->getChannelName()<<" "<<status<<"\n";
        }
    }

    virtual void getDone(
        const epics::pvData::Status& status,
        pva::ChannelGet::shared_pointer const & channelGet,
        pvd::PVStructure::shared_pointer const & pvStructure,
        pvd::BitSet::shared_pointer const & bitSet)
    {
        if(status.isSuccess()) {
            pvd::PVFieldPtr valfld(pvStructure->getSubField("value"));
            if(!valfld)
                valfld = pvStructure;
            std::cout<<channelGet->getChannel()->getChannelName()<<" : "<<*valfld<<"\n";
        } else {
            std::cout<<"Oops Get: "<<status<<"\n";
        }
    }
};

} // namespace

int main(int argc, char *argv[]) {
    try {
        // an empty structure
        pvd::PVStructurePtr pvReq(pvd::getPVDataCreate()->createPVStructure(
                                      pvd::getFieldCreate()->createFieldBuilder()->createStructure()
                                      ));
        pva::Configuration::shared_pointer conf(pva::ConfigurationBuilder()
                                                .push_env()
                                                .build());

        pva::ClientFactory::start();

        pva::ChannelProvider::shared_pointer provider(pva::getChannelProviderRegistry()->createProvider("pva", conf));
        if(!provider)
            throw std::logic_error("pva provider not registered");

        typedef std::set<pva::ChannelGet::shared_pointer> gets_t;
        gets_t gets;

        pva::ChannelRequester::shared_pointer chanreq(new ChanReq);
        pva::ChannelGetRequester::shared_pointer getreq(new GetReq);

        for(int i=1; i<argc; i++) {
            if(argv[i][0]=='-') continue;
            pva::Channel::shared_pointer chan(provider->createChannel(argv[i], chanreq));
            // if !chan then channelCreated() called with error status
            if(!chan) continue;

            pva::ChannelGet::shared_pointer op(chan->createChannelGet(getreq, pvReq));
            if(!op) continue;

            gets.insert(op);
            // drop our explicit Channel reference, ChannelGet holds an additional reference
        }

        done.wait();

        return 0;
    } catch(std::exception& e){
        std::cerr<<"Error: "<<e.what()<<"\n";
        return 1;
    }
}
