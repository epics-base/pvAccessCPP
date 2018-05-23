/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
#include <deque>
#include <set>
#include <vector>
#include <string>
#include <exception>

#if !defined(_WIN32)
#include <signal.h>
#define USE_SIGNAL
#endif

#include <epicsEvent.h>
#include <epicsThread.h>
#include <epicsMutex.h>
#include <epicsGuard.h>

#include <pv/configuration.h>
#include <pv/pvAccess.h>
#include <pv/serverContext.h>

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

namespace {

typedef epicsGuard<epicsMutex> Guard;

epicsEvent done;

bool debug;

#ifdef USE_SIGNAL
void alldone(int num)
{
    (void)num;
    done.signal();
}
#endif

pvd::Structure::const_shared_pointer spamtype(pvd::getFieldCreate()->createFieldBuilder()
                                              ->setId("epics:nt/NTScalar:1.0")
                                              ->add("value", pvd::pvInt)
                                              ->createStructure());

struct SpamProvider;
struct SpamChannel;

struct SpamSource : public pva::MonitorFIFO::Source
{
    epicsMutex mutex;

    epicsUInt32 counter;

    const pvd::PVStructurePtr cur;
    const pvd::PVIntPtr val;
    pvd::BitSet changed;

    SpamSource()
        :counter(0)
        ,cur(pvd::getPVDataCreate()->createPVStructure(spamtype))
        ,val(cur->getSubFieldT<pvd::PVInt>("value"))
    {
        changed.set(val->getFieldOffset()); // our value always changes
    }
    virtual ~SpamSource() {}
    virtual void freeHighMark(pva::MonitorFIFO *mon, size_t numEmpty) OVERRIDE FINAL
    {
        Guard G(mutex);
        for(;numEmpty; numEmpty--)
        {
            val->put(counter);
            if(!mon->tryPost(*cur, changed) && numEmpty!=1) {
                std::cerr<<"spam tryPost() inconsistent "<<numEmpty<<"\n";
                return;
            }
            counter++;
        }
    }
};

struct SpamChannel : public pva::Channel,
        public std::tr1::enable_shared_from_this<SpamChannel>
{
    const std::tr1::shared_ptr<pva::ChannelProvider> provider;
    const std::string name;
    const pva::ChannelRequester::weak_pointer requester;

    SpamChannel(const std::tr1::shared_ptr<pva::ChannelProvider>& provider,
                const std::string& name,
                const pva::ChannelRequester::shared_pointer& requester)
        :provider(provider)
        ,name(name)
        ,requester(requester)
    {}
    virtual ~SpamChannel() {}

    virtual std::string getChannelName() OVERRIDE FINAL {return name;}

    virtual std::string getRemoteAddress() OVERRIDE FINAL {return "";}
    virtual ConnectionState getConnectionState() OVERRIDE FINAL {return CONNECTED;}
    virtual pva::ChannelRequester::shared_pointer getChannelRequester() OVERRIDE FINAL { return pva::ChannelRequester::shared_pointer(requester); }

    virtual void destroy() OVERRIDE FINAL {}

    virtual std::tr1::shared_ptr<pva::ChannelProvider> getProvider() OVERRIDE FINAL {return provider;}

    virtual pva::AccessRights getAccessRights(pvd::PVField::shared_pointer const & pvField) OVERRIDE FINAL
    {
        return pva::readWrite;
    }

    virtual void getField(pva::GetFieldRequester::shared_pointer const & requester,std::string const & subField) OVERRIDE FINAL
    {
        requester->getDone(pvd::Status::Ok, spamtype);
    }

    virtual pva::Monitor::shared_pointer createMonitor(const pva::MonitorRequester::shared_pointer &requester,
                                                       const pvd::PVStructure::shared_pointer &pvRequest) OVERRIDE FINAL
    {
        std::tr1::shared_ptr<SpamSource> us(new SpamSource);
        std::tr1::shared_ptr<pva::MonitorFIFO> ret(new pva::MonitorFIFO(requester, pvRequest, us));
        // ret holds strong ref. to us
        ret->open(spamtype);
        ret->notify();
        return ret;
    }
};

struct SpamProvider : public pva::ChannelProvider,
                      public pva::ChannelFind,
                      public std::tr1::enable_shared_from_this<SpamProvider>
{
    const std::string channelName;
    SpamProvider(const std::string& name) :channelName(name) {}
    virtual ~SpamProvider() {}

    virtual std::string getProviderName() OVERRIDE FINAL {return "SpamProvider";}

    virtual void destroy() OVERRIDE FINAL {}

    virtual pva::ChannelFind::shared_pointer channelFind(std::string const & name,
                                                    pva::ChannelFindRequester::shared_pointer const & requester) OVERRIDE FINAL
    {
        std::cerr<<"XXX '"<<name<<"'\n";
        pva::ChannelFind::shared_pointer ret;
        if(name.size()>=this->channelName.size() && strncmp(name.c_str(), this->channelName.c_str(), this->channelName.size())==0) {
            ret = shared_from_this();
        }
        std::cout<<__FUNCTION__<<" "<<name<<" found="<<!!ret<<"\n";
        requester->channelFindResult(pvd::Status::Ok, ret, !!ret);
        return ret;
    }
    virtual std::tr1::shared_ptr<pva::ChannelProvider> getChannelProvider() OVERRIDE FINAL { return shared_from_this(); }
    virtual void cancel() OVERRIDE FINAL {}

    virtual pva::Channel::shared_pointer createChannel(std::string const & name,
                                                  pva::ChannelRequester::shared_pointer const & requester,
                                                  short priority, std::string const & address) OVERRIDE FINAL
    {
        std::tr1::shared_ptr<SpamChannel> ret;
        if(name.size()>=this->channelName.size() && strncmp(name.c_str(), this->channelName.c_str(), this->channelName.size())==0) {
            ret.reset(new SpamChannel(shared_from_this(), channelName, requester));
        }
        std::cout<<__FUNCTION__<<" "<<name<<" connect "<<ret.get()<<"\n";
        requester->channelCreated(ret ? pvd::Status::Ok : pvd::Status::error(""), ret);
        return ret;
    }
};

} // namespace

int main(int argc, char *argv[]) {
    try {
        for(int i=1; i<argc; i++) {
            if(strcmp("-d", argv[i])==0)
                debug = true;
        }
        std::tr1::shared_ptr<SpamProvider> provider(new SpamProvider("spam"));
        pva::ServerContext::shared_pointer server(pva::ServerContext::create(pva::ServerContext::Config()
                                                                             .provider(provider)
                                                                             .config(pva::ConfigurationBuilder()
                                                                                     .push_env()
                                                                                     .build())));
        std::cout<<"Server use_count="<<server.use_count()<<" provider use_count="<<provider.use_count()<<"\n";

#ifdef USE_SIGNAL
        signal(SIGINT, alldone);
        signal(SIGTERM, alldone);
        signal(SIGQUIT, alldone);
#endif
        server->printInfo();

        std::cout<<"Waiting\n";
        done.wait();
        std::cout<<"Done\n";

        std::cout<<"Server use_count="<<server.use_count()<<"\n"
                 <<show_referrers(server, false);
        server.reset();

        std::cout<<"threads\n";
        std::cout.flush();
        epicsThreadShowAll(0);

        std::cout<<"provider use_count="<<provider.use_count()<<"\n"
                 <<show_referrers(provider, false);
        if(!provider.unique())
            return 2;

    } catch(std::exception& e){
        std::cerr<<"Error: "<<e.what()<<"\n";
        return 1;
    }
    return 0;
}
