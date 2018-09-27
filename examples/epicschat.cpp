/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <stdio.h>

#include <sstream>
#include <map>

#if !defined(_WIN32)
#include <signal.h>
#define USE_SIGNAL
#endif

#include <epicsMutex.h>
#include <epicsGuard.h>
#include <epicsEvent.h>

#include <pv/pvData.h>
#include <pv/serverContext.h>
#include <pva/server.h>
#include <pva/sharedstate.h>

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

namespace {

epicsEvent done;

#ifdef USE_SIGNAL
void alldone(int num)
{
    (void)num;
    done.signal();
}
#endif

static pvd::StructureConstPtr string_type(pvd::getFieldCreate()->createFieldBuilder()
                                          ->add("value", pvd::pvString)
                                          ->createStructure());

struct ChatHandler : public pvas::SharedPV::Handler
{
    POINTER_DEFINITIONS(ChatHandler);
    virtual ~ChatHandler() {
        printf("Cleanup Room\n");
    }
    virtual void onLastDisconnect(const pvas::SharedPV::shared_pointer& self) OVERRIDE FINAL {
        printf("Close Room %p\n", &self);
    }
    virtual void onPut(const pvas::SharedPV::shared_pointer& self, pvas::Operation& op) OVERRIDE FINAL {
        pva::ChannelRequester::shared_pointer req(op.getChannel()->getChannelRequester());
        std::ostringstream strm;

        if(req) {
            strm<<req->getRequesterName()<<" says ";
        } else {
            op.complete(pvd::Status::error("Defunct Put"));
            return;
        }

        strm<<op.value().getSubFieldT<pvd::PVString>("value")->get();

        pvd::PVStructurePtr replacement(pvd::getPVDataCreate()->createPVStructure(string_type));

        replacement->getSubFieldT<pvd::PVString>("value")->put(strm.str());

        self->post(*replacement, op.changed());
        op.complete();
    }
};

struct RoomHandler : public pvas::DynamicProvider::Handler,
                     public std::tr1::enable_shared_from_this<RoomHandler>
{
    POINTER_DEFINITIONS(RoomHandler);

    const std::string prefix;

    mutable epicsMutex mutex;

    typedef std::map<std::string, pvas::SharedPV::weak_pointer> rooms_t;
    rooms_t rooms;

    RoomHandler(const std::string& prefix) :prefix(prefix) {}
    virtual ~RoomHandler() {}

    virtual void hasChannels(pvas::DynamicProvider::search_type& names) OVERRIDE FINAL {
        for(pvas::DynamicProvider::search_type::iterator it(names.begin()), end(names.end());
            it != end; ++it)
        {
            if(it->name().find(prefix)==0)
                it->claim();
        }
    }

    virtual std::tr1::shared_ptr<epics::pvAccess::Channel> createChannel(const std::tr1::shared_ptr<epics::pvAccess::ChannelProvider>& provider,
                                                                         const std::string& name,
                                                                         const std::tr1::shared_ptr<epics::pvAccess::ChannelRequester>& requester) OVERRIDE FINAL
    {
        pva::Channel::shared_pointer ret;

        pvas::SharedPV::shared_pointer pv;
        bool created = false;
        if(name.find(prefix)==0)
        {
            Guard G(mutex);

            rooms_t::iterator it(rooms.find(name));
            if(it!=rooms.end()) {
                // re-use existing?
                pv = it->second.lock();
            }

            // rather than deal with wrapped shared_ptr to remove PVs
            // as they are destroyed, just sweep each time a new channel is created
            for(rooms_t::iterator next(rooms.begin()), end(rooms.end()); next!=end;) {
                rooms_t::iterator cur(next++);
                if(cur->second.expired())
                    rooms.erase(cur);
            }

            if(!pv) {
                // nope
                ChatHandler::shared_pointer handler(new ChatHandler);
                pv = pvas::SharedPV::build(handler);

                rooms[name] = pv;
                created = true;
            }

        }
        // unlock

        if(pv) {
            if(created) {
                pv->open(string_type);

                // set a non-default initial value so that if we are connecting for
                // a get, then there will be something to be got.
                pvd::PVStructurePtr initial(pvd::getPVDataCreate()->createPVStructure(string_type));
                pvd::PVStringPtr value(initial->getSubFieldT<pvd::PVString>("value"));
                value->put("Created!");

                pv->post(*initial, pvd::BitSet().set(value->getFieldOffset()));
                printf("New Room: '%s' for %s as %p\n", name.c_str(), requester->getRequesterName().c_str(), pv.get());
            } else {
                printf("Attach Room: '%s' for %s as %p\n", name.c_str(), requester->getRequesterName().c_str(), pv.get());
            }

            ret = pv->connect(provider, name, requester);
        } else {
            // mis-matched prefix
        }

        return ret;
    }
};

}//namespace

int main(int argc, char *argv[])
{
    try {
        if(argc<=1) {
            fprintf(stderr, "Usage: %s <prefix>", argv[0]);
            return 1;
        }

        RoomHandler::shared_pointer handler(new RoomHandler(argv[1]));

        pvas::DynamicProvider provider("chat", handler);

        pva::ServerContext::shared_pointer server(pva::ServerContext::create(
                                                      pva::ServerContext::Config()
                                                      // use default config from environment
                                                      .provider(provider.provider())
                                                      ));

#ifdef USE_SIGNAL
        signal(SIGINT, alldone);
        signal(SIGTERM, alldone);
        signal(SIGQUIT, alldone);
#endif
        server->printInfo();

        done.wait();

    } catch(std::exception& e){
        std::cerr<<"Error: "<<e.what()<<"\n";
        return 1;
    }
    return 0;
}
