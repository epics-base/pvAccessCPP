/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <list>

#include <epicsMutex.h>
#include <epicsGuard.h>
#include <errlog.h>

#include <shareLib.h>
#include <pv/sharedPtr.h>
#include <pv/noDefaultMethods.h>
#include <pv/sharedVector.h>
#include <pv/bitSet.h>
#include <pv/pvData.h>
#include <pv/createRequest.h>
#include <pv/status.h>
#include <pv/reftrack.h>

#define epicsExportSharedSymbols
#include "sharedstateimpl.h"

namespace pvas {
namespace detail {

size_t SharedChannel::num_instances;


SharedChannel::SharedChannel(const std::tr1::shared_ptr<SharedPV> &owner,
                             const pva::ChannelProvider::shared_pointer provider,
                             const std::string& channelName,
                             const requester_type::shared_pointer& requester)
    :owner(owner)
    ,channelName(channelName)
    ,requester(requester)
    ,provider(provider)
    ,dead(false)
{
    REFTRACE_INCREMENT(num_instances);

    if(owner->debugLvl>5) {
        errlogPrintf("%s : Open channel to %s > %p\n",
                     requester->getRequesterName().c_str(),
                     channelName.c_str(),
                     this);
    }

    {
        Guard G(owner->mutex);
        owner->channels.push_back(this);
    }
}

SharedChannel::~SharedChannel()
{
    destroy();
    REFTRACE_DECREMENT(num_instances);
}

void SharedChannel::destroy()
{
    std::tr1::shared_ptr<SharedPV::Handler> handler;
    {
        Guard G(owner->mutex);

        if(dead) return;
        dead = true;

        bool wasempty = owner->channels.empty();
        owner->channels.remove(this);
        if(!wasempty && owner->channels.empty() && owner->notifiedConn) {
            handler = owner->handler;
            owner->notifiedConn = false;
        }
    }
    if(handler) {
        handler->onLastDisconnect(owner);
    }
    if(owner->debugLvl>5)
    {
        pva::ChannelRequester::shared_pointer req(requester.lock());
        errlogPrintf("%s : Close channel to %s > %p\n",
                     req ? req->getRequesterName().c_str() : "<Defunct>",
                     channelName.c_str(),
                     this);
    }
}

std::tr1::shared_ptr<pva::ChannelProvider> SharedChannel::getProvider()
{
    return provider.lock();
}

std::string SharedChannel::getRemoteAddress()
{
    return getChannelName(); // for lack of anything better to do...
}

std::string SharedChannel::getChannelName()
{
    return channelName;
}

std::tr1::shared_ptr<pva::ChannelRequester> SharedChannel::getChannelRequester()
{
    return requester.lock();
}

void SharedChannel::getField(pva::GetFieldRequester::shared_pointer const & requester,std::string const & subField)
{
    pvd::FieldConstPtr desc;
    pvd::Status sts;
    SharedPV::Handler::shared_pointer handler;
    {
        Guard G(owner->mutex);
        if(dead) {
            sts = pvd::Status::error("Dead Channel");

        } else {
            if(owner->type) {
                desc = owner->type;
            }

            if(!owner->channels.empty() && !owner->notifiedConn) {
                handler = owner->handler;
                owner->notifiedConn = true;
            }
            owner->getfields.push_back(requester);
        }
    }
    if(desc || !sts.isOK()) {
        requester->getDone(sts, desc);
    }
    if(handler) {
        handler->onFirstConnect(owner);
    }
}

pva::ChannelPut::shared_pointer SharedChannel::createChannelPut(
        pva::ChannelPutRequester::shared_pointer const & requester,
        pvd::PVStructure::shared_pointer const & pvRequest)
{
    std::tr1::shared_ptr<SharedPut> ret(new SharedPut(shared_from_this(), requester, pvRequest));

    pvd::StructureConstPtr type;
    pvd::Status sts;
    std::string warning;
    SharedPV::Handler::shared_pointer handler;
    try {
        {
            Guard G(owner->mutex);
            if(dead) {
                sts = pvd::Status::error("Dead Channel");

            } else {
                // ~SharedPut removes
                owner->puts.push_back(ret.get());
                if(owner->current) {
                    ret->mapper.compute(*owner->current, *pvRequest, owner->config.mapperMode);
                    type = ret->mapper.requested();
                    warning = ret->mapper.warnings();
                }

                if(!owner->channels.empty() && !owner->notifiedConn) {
                    handler = owner->handler;
                    owner->notifiedConn = true;
                }
            }
        }
        if(!warning.empty())
            requester->message(warning, pvd::warningMessage);
        if(type || !sts.isOK())
            requester->channelPutConnect(sts, ret, type);
    }catch(std::runtime_error& e){
        ret.reset();
        type.reset();
        requester->channelPutConnect(pvd::Status::error(e.what()), ret, type);
    }
    if(handler) {
        handler->onFirstConnect(owner);
    }
    return ret;
}

pva::ChannelRPC::shared_pointer SharedChannel::createChannelRPC(
        pva::ChannelRPCRequester::shared_pointer const & requester,
        pvd::PVStructure::shared_pointer const & pvRequest)
{
    std::tr1::shared_ptr<SharedRPC> ret(new SharedRPC(shared_from_this(), requester, pvRequest));
    ret->connected = true;

    pvd::Status sts;
    {
        Guard G(owner->mutex);
        if(dead) {
            sts = pvd::Status::error("Dead Channel");

        } else {
            owner->rpcs.push_back(ret.get());
        }
    }
    requester->channelRPCConnect(sts, ret);
    return ret;
}

pva::Monitor::shared_pointer SharedChannel::createMonitor(
        pva::MonitorRequester::shared_pointer const & requester,
        pvd::PVStructure::shared_pointer const & pvRequest)
{
    SharedMonitorFIFO::Config mconf;
    SharedPV::Handler::shared_pointer handler;
    mconf.dropEmptyUpdates = owner->config.dropEmptyUpdates;
    mconf.mapperMode = owner->config.mapperMode;

    std::tr1::shared_ptr<SharedMonitorFIFO> ret(new SharedMonitorFIFO(shared_from_this(), requester, pvRequest, &mconf));

    bool notify;
    pvd::Status sts;
    {
        Guard G(owner->mutex);
        if(dead) {
            sts = pvd::Status::error("Dead Channel");
            notify = false;

        } else {
            owner->monitors.push_back(ret.get());
            notify = !!owner->type;
            if(notify) {
                ret->open(owner->type);
                // post initial update
                ret->post(*owner->current, owner->valid);
            }

            if(!owner->channels.empty() && !owner->notifiedConn) {
                handler = owner->handler;
                owner->notifiedConn = true;
            }
        }
    }
    if(!sts.isOK()) {
        requester->monitorConnect(sts, pvd::MonitorPtr(), pvd::StructureConstPtr());
        ret.reset();

    } else {
        if(notify) {
            ret->notify();
        }
        if(handler) {
            handler->onFirstConnect(owner);
        }
    }
    return ret;
}


SharedMonitorFIFO::SharedMonitorFIFO(const std::tr1::shared_ptr<SharedChannel>& channel,
                                     const requester_type::shared_pointer& requester,
                                     const pvd::PVStructure::const_shared_pointer &pvRequest,
                                     Config *conf)
    :pva::MonitorFIFO(requester, pvRequest, pva::MonitorFIFO::Source::shared_pointer(), conf)
    ,channel(channel)
{}

SharedMonitorFIFO::~SharedMonitorFIFO()
{
    Guard G(channel->owner->mutex);
    channel->owner->monitors.remove(this);
}

} // namespace detail

Operation::Operation(const std::tr1::shared_ptr<Impl> impl)
    :impl(impl)
{}

const epics::pvData::PVStructure& Operation::pvRequest() const
{
    return *impl->pvRequest;
}

const epics::pvData::PVStructure& Operation::value() const
{
    return *impl->value;
}

const epics::pvData::BitSet& Operation::changed() const
{
    return impl->changed;
}

std::string Operation::channelName() const
{
    std::string ret;
    std::tr1::shared_ptr<epics::pvAccess::Channel> chan(impl->getChannel());
    if(chan) {
        ret = chan->getChannelName();
    }
    return ret;
}

const pva::PeerInfo* Operation::peer() const
{
    return impl->info ? impl->info.get() : 0;
}

void Operation::complete()
{
    impl->complete(pvd::Status(), 0);
}

void Operation::complete(const epics::pvData::Status& sts)
{
    impl->complete(sts, 0);
}

void Operation::complete(const epics::pvData::PVStructure &value,
                         const epics::pvData::BitSet &changed)
{
    impl->complete(pvd::Status(), &value);
}

void Operation::info(const std::string& msg)
{
    pva::ChannelBaseRequester::shared_pointer req(impl->getRequester());
    if(req)
        req->message(msg, pvd::infoMessage);
}

void Operation::warn(const std::string& msg)
{
    pva::ChannelBaseRequester::shared_pointer req(impl->getRequester());
    if(req)
        req->message(msg, pvd::warningMessage);
}

int Operation::isDebug() const
{
    Guard G(impl->mutex);
    return impl->debugLvl;
}

std::tr1::shared_ptr<epics::pvAccess::Channel> Operation::getChannel()
{
    return impl->getChannel();
}

std::tr1::shared_ptr<pva::ChannelBaseRequester> Operation::getRequester()
{
    return impl->getRequester();
}

bool Operation::valid() const
{
    return !!impl;
}

void Operation::Impl::Cleanup::operator ()(Operation::Impl* impl) {
    bool err;
    {
        Guard G(impl->mutex);
        err = !impl->done;
    }
    if(err)
        impl->complete(pvd::Status::error("Implicit Cancel"), 0);

    delete impl;
}

} // namespace pvas
