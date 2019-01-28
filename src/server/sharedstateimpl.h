/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
#ifndef SHAREDSTATEIMPL_H
#define SHAREDSTATEIMPL_H

#include <pv/createRequest.h>

#include "pva/sharedstate.h"
#include <pv/pvAccess.h>
#include <pv/security.h>
#include <pv/reftrack.h>

#define FOR_EACH(TYPE, IT, END, OBJ) for(TYPE IT((OBJ).begin()), END((OBJ).end()); IT != END; ++IT)

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

namespace pvas {
namespace detail {

struct SharedChannel : public pva::Channel,
                       public std::tr1::enable_shared_from_this<SharedChannel>
{
    static size_t num_instances;

    const std::tr1::shared_ptr<SharedPV> owner;
    const std::string channelName;
    const requester_type::weak_pointer requester;
    const pva::ChannelProvider::weak_pointer provider;

    bool dead; // has destroy() been called?

    SharedChannel(const std::tr1::shared_ptr<SharedPV>& owner,
                  const pva::ChannelProvider::shared_pointer provider,
                  const std::string& channelName,
                  const requester_type::shared_pointer& requester);
    virtual ~SharedChannel();

    virtual void destroy() OVERRIDE FINAL;

    virtual std::tr1::shared_ptr<pva::ChannelProvider> getProvider() OVERRIDE FINAL;
    virtual std::string getRemoteAddress() OVERRIDE FINAL;
    virtual std::string getChannelName() OVERRIDE FINAL;
    virtual std::tr1::shared_ptr<pva::ChannelRequester> getChannelRequester() OVERRIDE FINAL;

    virtual void getField(pva::GetFieldRequester::shared_pointer const & requester,std::string const & subField) OVERRIDE FINAL;

    virtual pva::ChannelPut::shared_pointer createChannelPut(
            pva::ChannelPutRequester::shared_pointer const & requester,
            pvd::PVStructure::shared_pointer const & pvRequest) OVERRIDE FINAL;

    virtual pva::ChannelRPC::shared_pointer createChannelRPC(
            pva::ChannelRPCRequester::shared_pointer const & requester,
            pvd::PVStructure::shared_pointer const & pvRequest) OVERRIDE FINAL;

    virtual pva::Monitor::shared_pointer createMonitor(
            pva::MonitorRequester::shared_pointer const & requester,
            pvd::PVStructure::shared_pointer const & pvRequest) OVERRIDE FINAL;
};

struct SharedMonitorFIFO : public pva::MonitorFIFO
{
    const std::tr1::shared_ptr<SharedChannel> channel;
    SharedMonitorFIFO(const std::tr1::shared_ptr<SharedChannel>& channel,
                      const requester_type::shared_pointer& requester,
                      const pvd::PVStructure::const_shared_pointer &pvRequest,
                      Config *conf);
    virtual ~SharedMonitorFIFO();
};

struct SharedPut : public pva::ChannelPut,
                   public std::tr1::enable_shared_from_this<SharedPut>
{
    const std::tr1::shared_ptr<SharedChannel> channel;
    const requester_type::weak_pointer requester;
    const pvd::PVStructure::const_shared_pointer pvRequest;

    // guarded by PV mutex
    pvd::PVRequestMapper mapper;

    static size_t num_instances;

    SharedPut(const std::tr1::shared_ptr<SharedChannel>& channel,
              const requester_type::shared_pointer& requester,
              const pvd::PVStructure::const_shared_pointer &pvRequest);
    virtual ~SharedPut();

    virtual void destroy() OVERRIDE FINAL;
    virtual std::tr1::shared_ptr<pva::Channel> getChannel() OVERRIDE FINAL;
    virtual void cancel() OVERRIDE FINAL;
    virtual void lastRequest() OVERRIDE FINAL;

    virtual void put(
        epics::pvData::PVStructure::shared_pointer const & pvPutStructure,
        epics::pvData::BitSet::shared_pointer const & putBitSet) OVERRIDE FINAL;

    virtual void get() OVERRIDE FINAL;
};

struct SharedRPC : public pva::ChannelRPC,
                   public std::tr1::enable_shared_from_this<SharedRPC>
{
    const std::tr1::shared_ptr<SharedChannel> channel;
    const requester_type::weak_pointer requester;
    const pvd::PVStructure::const_shared_pointer pvRequest;

    static size_t num_instances;

    bool connected; // have I called requester->channelRPCConnect(Ok) ?

    SharedRPC(const std::tr1::shared_ptr<SharedChannel>& channel,
              const requester_type::shared_pointer& requester,
              const pvd::PVStructure::const_shared_pointer &pvRequest);
    virtual ~SharedRPC();

    virtual void destroy() OVERRIDE FINAL;
    virtual std::tr1::shared_ptr<pva::Channel> getChannel() OVERRIDE FINAL;
    virtual void cancel() OVERRIDE FINAL;
    virtual void lastRequest() OVERRIDE FINAL;

    virtual void request(epics::pvData::PVStructure::shared_pointer const & pvArgument) OVERRIDE FINAL;
};

} // namespace detail

struct Operation::Impl
{
    static size_t num_instances;

    epicsMutex mutex;

    const pvd::PVStructure::const_shared_pointer pvRequest, value;
    const pvd::BitSet changed;
    //! const after sub-class ctor
    pva::PeerInfo::const_shared_pointer info;

    bool done;
    int debugLvl;

    Impl(const pvd::PVStructure::const_shared_pointer& pvRequest,
         const pvd::PVStructure::const_shared_pointer& value,
         const pvd::BitSet& changed,
         int debugLvl = 0)
        :pvRequest(pvRequest), value(value), changed(changed), done(false), debugLvl(debugLvl)
    {}
    virtual ~Impl() {}

    virtual pva::Channel::shared_pointer getChannel() =0;
    virtual pva::ChannelBaseRequester::shared_pointer getRequester() =0;
    virtual void complete(const pvd::Status& sts,
                          const epics::pvData::PVStructure* value) =0;

    struct Cleanup {
        void operator()(Impl*);
    };
};

} // namespace pvas

#endif // SHAREDSTATEIMPL_H
