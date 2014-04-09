/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvAccessCPP is distributed subject to a Software License Agreement
 * found in file LICENSE that is included with this distribution.
 */

/**
 * @authors mrk, mse 
 */
/* Author Marty Kraimer 2011.12 */


#include <iostream>
#include <string>

#include <pv/pvData.h>
#include <pv/convert.h>

#include <pv/clientFactory.h>
#include <pv/event.h>
#include <pv/logger.h>
#include <pv/rpcService.h>

#include "rpcClient.h"

using namespace epics::pvData;
using std::tr1::static_pointer_cast;
using std::cout;
using std::endl;

namespace epics { namespace pvAccess { 

RPCClientPtr RPCClient::create(String const &channelName,PVStructurePtr const &pvRequest)
{
    return RPCClientPtr(new RPCClient(channelName,pvRequest));
}

PVStructurePtr RPCClient::request(
    String const &channelName,
    PVStructure::shared_pointer const & pvArgument,
    double timeOut)
{
    ClientFactory::start();
    RPCClientPtr client = RPCClient::create(channelName,PVStructurePtr());
    if(!client->connect(timeOut)) throw std::runtime_error(client->getMessage());
    PVStructurePtr pvResult = client->request(pvArgument,true);
    if(pvResult==NULL) throw std::runtime_error(client->getMessage());
    ClientFactory::stop();
    return pvResult;
}


RPCClient::RPCClient(
    String const &channelName,
    PVStructure::shared_pointer const &pvRequest)
: channelName(channelName),
  pvRequest(pvRequest),
  requesterName("RPCClient"),
  isOK(true)
{
}

RPCClient::~RPCClient()
{
//printf("RPCClient::~RPCClient()\n");
}

void RPCClient::destroy()
{
    Lock xx(mutex);
    if(channel.get()!=0) {
        channel->destroy();
        channel.reset();
        pvRequest.reset();
        channelRPC.reset();
        pvResponse.reset();
    }
}

bool RPCClient::connect(double timeOut)
{
    if(pvRequest.get()==NULL) {
        pvRequest = getCreateRequest()->createRequest(
            "record[process=true]field()",getPtrSelf());
    }
    issueConnect();
    return waitConnect(timeOut);
}

void RPCClient::issueConnect()
{
    event.tryWait(); // make sure event is empty
    ChannelAccess::shared_pointer const &channelAccess = getChannelAccess();
    ChannelProvider::shared_pointer const &channelProvider
         = channelAccess->getProvider(String("pva"));
    if(channelProvider==NULL) {
        String message("Provider pva not active. Did You call ClientFactory::start()?");
        throw std::runtime_error(message);
    }
    channel = channelProvider->createChannel(
        channelName,
        getPtrSelf(),
        ChannelProvider::PRIORITY_DEFAULT);
}

bool RPCClient::waitConnect(double timeOut) {
    // wait for channel to connect
    bool ok = event.wait(timeOut);
    if(!ok) return ok;
    channelRPC = channel->createChannelRPC(getPtrSelf(),pvRequest);
    event.wait();
    return isOK;
}

epics::pvData::PVStructure::shared_pointer RPCClient::request(
    PVStructure::shared_pointer const & pvArgument,
    bool lastRequest)
{
//printf("RPCClient::request\n");
    issueRequest(pvArgument,lastRequest);
//printf("RPCClient::request calling waitRequest\n");
    return waitRequest();
}

void RPCClient::issueRequest(
    PVStructure::shared_pointer const & pvArgument,
    bool lastRequest)
{
//printf("RPCClient::issueRequest\n");
    event.tryWait(); // make sure event is empty
//printf("RPCClient::issueRequest calling channelRPC->request\n");
    channelRPC->request(pvArgument,lastRequest);
}

epics::pvData::PVStructure::shared_pointer RPCClient::waitRequest()
{
//printf("calling event.wait()\n");
    bool ok = event.wait();
//printf("wait returned %s\n",(ok==true ? "true" : "false"));
    if(!ok) {
        isOK = false;
        lastMessage = "event.wait failed\n";
        pvResponse = epics::pvData::PVStructure::shared_pointer();
    }
    return pvResponse;
}

String RPCClient::getMessage() { return lastMessage;}


void RPCClient::channelCreated(
    const Status& status,
    Channel::shared_pointer const & channel)
{
//printf("RPCClient::channelCreate\n");
    isOK = status.isOK();
    this->channel = channel;
    if(!isOK) {
        String message = Status::StatusTypeName[status.getType()];
        message += " " + status.getMessage();
        lastMessage = message;
        event.signal();
    }
}

void RPCClient::channelStateChange(
    Channel::shared_pointer const & channel,
    Channel::ConnectionState connectionState)
{
    this->channel = channel;
    this->connectionState = connectionState;
    event.signal();
}

String RPCClient::getRequesterName(){ return requesterName;}

void RPCClient::message(String const &message,MessageType messageType)
{
    lastMessage = getMessageTypeName(messageType);
    lastMessage += " " + message;
}

void RPCClient::channelRPCConnect(
    const Status& status,
    ChannelRPC::shared_pointer const & channelRPC)
{
    this->channelRPC = channelRPC;
    if(!status.isOK()) {
        isOK = false;
        String message = Status::StatusTypeName[status.getType()];
        message += " " + status.getMessage();
        lastMessage = message;
    }
    event.signal();
}

void RPCClient::requestDone(
    const Status& status,
    PVStructure::shared_pointer const & pvResponse)
{
//printf("RPCClient::requestDone\n");
    this->pvResponse = pvResponse;
    if(!status.isOK()) {
        isOK = false;
        String message = Status::StatusTypeName[status.getType()];
        message += " " + status.getMessage();
        lastMessage = message;
    }
    event.signal();
}
    

}}
