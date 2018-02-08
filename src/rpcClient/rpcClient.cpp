/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvAccessCPP is distributed subject to a Software License Agreement
 * found in file LICENSE that is included with this distribution.
 */

#include <iostream>
#include <string>

#include <epicsEvent.h>
#include <pv/pvData.h>
#include <pv/event.h>
#include <pv/current_function.h>

#define epicsExportSharedSymbols
#include <pv/pvAccess.h>
#include <pv/clientFactory.h>
#include <pv/logger.h>
#include <pv/rpcService.h>

#include "pv/rpcClient.h"

#if 0
#  define TRACE(msg) std::cerr<<"TRACE: "<<CURRENT_FUNCTION<<" : "<< msg <<"\n"
#else
#  define TRACE(msg)
#endif

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

namespace epics{namespace pvAccess{

struct RPCClient::RPCRequester : public pva::ChannelRPCRequester
{
    POINTER_DEFINITIONS(RPCRequester);

    pvd::Mutex mutex;
    ChannelRPC::shared_pointer op;
    pvd::Status conn_status, resp_status;
    epics::pvData::PVStructure::shared_pointer next_args, last_data;
    epicsEvent event;
    bool inprogress, last;

    RPCRequester()
        :conn_status(pvd::Status::error("Never connected"))
        ,resp_status(pvd::Status::error("Never connected"))
        ,inprogress(false)
        ,last(false)
    {}
    virtual ~RPCRequester() {}

    virtual std::string getRequesterName() { return "RPCClient::RPCRequester"; }

    virtual void channelRPCConnect(
        const pvd::Status& status,
        ChannelRPC::shared_pointer const & operation)
    {
        bool lastreq, inprog;
        pvd::PVStructure::shared_pointer args;
        {
            pvd::Lock L(mutex);
            TRACE("status="<<status);
            op = operation;
            conn_status = status;
            args.swap(next_args);
            lastreq = last;
            inprog = inprogress;
            if(inprog && args)
                TRACE("request deferred: "<<args);
        }
        if(inprog && args) {
            TRACE("request deferred: "<<args);
            if(lastreq)
                operation->lastRequest();
            operation->request(args);
        }
        event.signal();
    }

    virtual void requestDone(
        const pvd::Status& status,
        ChannelRPC::shared_pointer const & operation,
        pvd::PVStructure::shared_pointer const & pvResponse)
    {
        TRACE("status="<<status<<" response:\n"<<pvResponse<<"\n");
        {
            pvd::Lock L(mutex);
            if(!inprogress) {
                std::cerr<<"pva provider give RPC requestDone() when no request in progress\n";
            } else {
                resp_status = status;
                last_data = pvResponse;
                if(resp_status.isSuccess() && !last_data) {
                    resp_status = pvd::Status::error("No reply data");
                }
                inprogress = false;
            }
        }
        event.signal();
    }

    virtual void channelDisconnect(bool destroy)
    {
        TRACE("destroy="<<destroy);
        {
            pvd::Lock L(mutex);
            resp_status = conn_status = pvd::Status::error("Connection lost");
            last_data.reset();
            next_args.reset();
            inprogress = false;
        }
        event.signal();
    }
};


RPCClient::RPCClient(const std::string & serviceName,
                     pvd::PVStructure::shared_pointer const & pvRequest,
                     const ChannelProvider::shared_pointer &provider,
                     const std::string &address)
    : m_serviceName(serviceName)
    , m_provider(provider)
    , m_pvRequest(pvRequest ? pvRequest : pvd::createRequest(""))
{
    ClientFactory::start();
    if(!m_provider)
        m_provider = ChannelProviderRegistry::clients()->getProvider("pva");
    if(!m_provider)
        throw std::logic_error("Unknown Provider");

    m_channel = m_provider->createChannel(serviceName, DefaultChannelRequester::build(),
                                        ChannelProvider::PRIORITY_DEFAULT,
                                        address);

    if(!m_channel)
        throw std::logic_error("provider createChannel() succeeds w/ NULL Channel");

    m_rpc_requester.reset(new RPCRequester);
    m_rpc = m_channel->createChannelRPC(m_rpc_requester, m_pvRequest);
    if(!m_rpc)
        throw std::logic_error("channel createChannelRPC() NULL");
}

void RPCClient::destroy()
{
    if (m_channel)
    {
        m_channel->destroy();
        m_channel.reset();
    }
    if (m_rpc)
    {
        m_rpc->destroy();
        m_rpc.reset();
    }
}

bool RPCClient::connect(double timeout)
{
    issueConnect();
    return waitConnect(timeout);
}

void RPCClient::issueConnect()
{
}

bool RPCClient::waitConnect(double timeout)
{
    pvd::Lock L(m_rpc_requester->mutex);
    TRACE("timeout="<<timeout);
    while(!m_rpc_requester->conn_status.isSuccess()) {
        L.unlock();
        if(!m_rpc_requester->event.wait(timeout)) {
            TRACE("TIMEOUT");
            return false;
        }
        L.lock();
    }
    TRACE("Connected");
    return true;
}



pvd::PVStructure::shared_pointer RPCClient::request(
    pvd::PVStructure::shared_pointer const & pvArgument,
    double timeout,
    bool lastRequest)
{
    if (connect(timeout))
    {
        issueRequest(pvArgument, lastRequest);
        return waitResponse(timeout);       // TODO reduce timeout for a time spent on connect
    }
    else
        throw epics::pvAccess::RPCRequestException(pvd::Status::STATUSTYPE_ERROR, "connection timeout");
}

void RPCClient::issueRequest(
    pvd::PVStructure::shared_pointer const & pvArgument,
    bool lastRequest)
{
    {
        pvd::Lock L(m_rpc_requester->mutex);
        TRACE("conn_status="<<m_rpc_requester->conn_status
            <<" resp_status="<<m_rpc_requester->resp_status
            <<" args:\n"<<pvArgument);
        if(m_rpc_requester->inprogress)
            throw std::logic_error("Request already in progress");
        m_rpc_requester->inprogress = true;
        m_rpc_requester->resp_status = pvd::Status::error("No Data");
        if(!m_rpc_requester->conn_status.isSuccess()) {
            TRACE("defer");
            m_rpc_requester->last = lastRequest;
            m_rpc_requester->next_args = pvArgument;
            return;
        }
        TRACE("request args: "<<pvArgument);
    }
    if(lastRequest)
        m_rpc->lastRequest();
    m_rpc->request(pvArgument);
}

pvd::PVStructure::shared_pointer RPCClient::waitResponse(double timeout)
{
    pvd::Lock L(m_rpc_requester->mutex);
    TRACE("timeout="<<timeout);

    while(m_rpc_requester->inprogress)
    {
        L.unlock();
        if(!m_rpc_requester->event.wait(timeout)) {
            TRACE("TIMEOUT");
            throw RPCRequestException(pvd::Status::STATUSTYPE_ERROR, "RPC timeout");
        }
        L.lock();
    }
    TRACE("Complete: conn_status="<<m_rpc_requester->conn_status
                 <<" resp_status="<<m_rpc_requester->resp_status
                 <<" data:\n"<<m_rpc_requester->last_data);

    if(!m_rpc_requester->conn_status.isSuccess())
        throw RPCRequestException(pvd::Status::STATUSTYPE_ERROR, m_rpc_requester->conn_status.getMessage());

    if(!m_rpc_requester->resp_status.isSuccess())
        throw RPCRequestException(pvd::Status::STATUSTYPE_ERROR, m_rpc_requester->resp_status.getMessage());

    // consume last_data so that we can't possibly return it twice
    pvd::PVStructure::shared_pointer data;
    data.swap(m_rpc_requester->last_data);

    if(!data)
        throw std::logic_error("No request in progress");

    // copy it so that the caller need not worry about whether it will overwritten
    // when the next request is issued
    pvd::PVStructure::shared_pointer ret(pvd::getPVDataCreate()->createPVStructure(data->getStructure()));
    ret->copyUnchecked(*data);

    return ret;
}

RPCClient::shared_pointer RPCClient::create(const std::string & serviceName,
        pvd::PVStructure::shared_pointer const & pvRequest)
{
    return RPCClient::shared_pointer(new RPCClient(serviceName, pvRequest));
}


}}// namespace epics::pvAccess
