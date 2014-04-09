/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvAccessCPP is distributed subject to a Software License Agreement
 * found in file LICENSE that is included with this distribution.
 */

#ifndef RPCCLIENT_H
#define RPCCLIENT_H

#include <string>

#include <pv/pvData.h>
#include <pv/pvAccess.h>


namespace epics { namespace pvAccess {

class RPCClient;
typedef std::tr1::shared_ptr<RPCClient> RPCClientPtr;

/**
 * This class provides an easy way to make a channelRPC request.
 * The client provides a PVStructure that is the argument for the
 * channelRPC request and receives a PVStructure that holds the result.
 * @author mrk
 *
 */
class RPCClient :
    public ChannelRequester,
    public ChannelRPCRequester,
    public std::tr1::enable_shared_from_this<RPCClient>
{
public:
    POINTER_DEFINITIONS(RPCClient);
    /** Factory
     * @param channelName The name of the channelRPC server.
     * @param pvRequest A request structure to pass to the server.
     * &returns RPCClientPtr.
     */
    static RPCClientPtr create(
        epics::pvData::String const &channelName,
        epics::pvData::PVStructurePtr const &pvRequest);
    /**
     * Make a channelRPC request.
     * This methods create a RPCClientPtr, connects to the server, makes a request 
     * and returns the result.
     * If any problem occurs an exception is thrown.
     * @param channelName The name of the channelRPC server.
     * @param pvArgument The argument to pass to the server.
     * @param timeOut The timeOut for connect.
     * @returns the result.
     */
    static epics::pvData::PVStructurePtr  request(
        epics::pvData::String const &channelName,
        epics::pvData::PVStructurePtr const & pvArgument,
        double timeOut = 1.0);
    /**
     * Destructor
     */
    virtual ~RPCClient();
    /** destroy the channelRPC.
     * This will clean up all resources used by the channelRPC
     */
    void destroy();
    /**
     * Connect to the server.
     * The method blocks until the connection is made or a timeout occurs.
     * It is the same as calling issueConnect and then waitConnect.
     * @param timeout Timeout in seconds to wait.
     * @returns (false,true) If (not connected, is connected).
     * If false then connect must be reissued.
     */
    bool connect(double timeout = 0.0);
    /**
     * Issue a connect request and return immediately.
     * waitConnect must be called to complete the request.
     */
    void issueConnect();
    /**
     * Wait for the connect request to complete.
     * @param timeout timeout in seconds to wait.
     * @returns (false,true) If (not connected, is connected).
     * If false then connect must be reissued.
     */
    bool waitConnect(double timeout = 0.0);
    /**
     * Make a channelRPC request.
     * @param pvArgument The argument to pass to the server.
     * @param lastRequest If true an automatic destroy is made.
     * @returns the result.
     * If the result is null then getMessage can be called to get the reason.
     */
    epics::pvData::PVStructurePtr  request(
        epics::pvData::PVStructurePtr const & pvArgument,
        bool lastRequest);
    /**
     * Issue a channelRPC request and return immediately.
     * waitRequest must be called to complete the request.
     * @param pvAgument The argument to pass to the server.
     * @param lastRequest If true an automatic destroy is made.
     */
    void  issueRequest(
        epics::pvData::PVStructurePtr const & pvArgument,
        bool lastRequest);
    /**
     * Wait for the request to complete.
     * @returns the result.s
     * If the result is null then getMessage can be called to get the reason.
     */
    epics::pvData::PVStructurePtr  waitRequest();
    /**
     * Get the reason why a connect or request failed.
     * @returns the message.
     */
    epics::pvData::String getMessage();
    // remaining methods are callbacks, i.e. not called by user code
    virtual void channelCreated(
        epics::pvData::Status const & status,
        Channel::shared_pointer const & channel);
    virtual void channelStateChange(
        Channel::shared_pointer const & channel,
        Channel::ConnectionState connectionState);
    virtual epics::pvData::String getRequesterName();
    virtual void message(
        epics::pvData::String const & message,
        epics::pvData::MessageType messageType);
    virtual void channelRPCConnect(
        epics::pvData::Status const & status,
        ChannelRPC::shared_pointer const & channelRPC);
    virtual void requestDone(
        epics::pvData::Status const & status,
        epics::pvData::PVStructurePtr const & pvResponse);
private:
    shared_pointer getPtrSelf()
    {
        return shared_from_this();
    }
    RPCClient(
        epics::pvData::String const &channelName,
        epics::pvData::PVStructurePtr const & pvRequest);
    epics::pvData::String channelName;
    epics::pvData::PVStructurePtr pvRequest;
    epics::pvData::String requesterName;
    bool isOK;
    epics::pvData::Event event;
    epics::pvData::Mutex mutex;
    epics::pvData::String lastMessage;
    Channel::shared_pointer channel;
    Channel::ConnectionState connectionState;
    ChannelRPC::shared_pointer channelRPC;
    epics::pvData::PVStructurePtr pvResponse;
};

}}

#endif  /* RPCCLIENT_H */


