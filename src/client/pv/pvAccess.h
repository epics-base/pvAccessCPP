/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVACCESS_H
#define PVACCESS_H

#include <vector>

#ifdef epicsExportSharedSymbols
#   define pvAccessEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/pvData.h>
#include <pv/createRequest.h>
#include <pv/status.h>
#include <pv/destroyable.h>
#include <pv/monitor.h>
#include <pv/bitSet.h>

#ifdef pvAccessEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef pvAccessEpicsExportSharedSymbols
#endif

#include <pv/pvaVersion.h>

#include <shareLib.h>

namespace epics {
namespace pvAccess {
class Configuration;

// TODO add write-only?
// change names
enum AccessRights {
    /**
     * Neither read or write access is allowed.
     */
    none,
    /**
     * Read access is allowed but write access is not allowed.
     */
    read,
    /**
      * Both read and write access are allowed.
      */
    readWrite
};


/**
 *
 */
class epicsShareClass Lockable
{
public:
    POINTER_DEFINITIONS(Lockable);

    virtual ~Lockable() {};

    virtual void lock() = 0;
    virtual void unlock() = 0;
};

/**
 * Scope lock.
 */
class epicsShareClass ScopedLock : private epics::pvData::NoDefaultMethods {
public:

    explicit ScopedLock(Lockable::shared_pointer const & li)
        : lockable(li), locked(true) {
        lockable->lock();
    }

    ~ScopedLock() {
        unlock();
    }

    void lock() {
        if(!locked) {
            lockable->lock();
            locked = true;
        }
    }

    void unlock() {
        if(locked) {
            lockable->unlock();
            locked=false;
        }
    }

    bool ownsLock() const {
        return locked;
    }

private:

    Lockable::shared_pointer const lockable;
    bool locked;
};


class Channel;
class ChannelProvider;

/**
 * Base interface for all channel requests.
 */
class epicsShareClass ChannelRequest : public epics::pvData::Destroyable, public Lockable, private epics::pvData::NoDefaultMethods {
public:
    POINTER_DEFINITIONS(ChannelRequest);

    /**
     * Get a channel instance this request belongs to.
     * @return the channel instance.
     */
    virtual std::tr1::shared_ptr<Channel> getChannel() = 0;

    /**
     * Cancel any pending request.
     * Completion will be reported via request's response callback:
     * <ul>
     *   <li>if cancel() request is issued after the request was already complete, request success/failure completion will be reported and cancel() request ignored.</li>
     *   <li>if the request was actually canceled, cancellation completion is reported.</li>
     * </ul>
     */
    virtual void cancel() = 0;

    /**
     * Announce next request as last request.
     * When last request will be completed (regardless of completion status) the remote and local instance will be destroyed.
     */
    virtual void lastRequest() = 0;
};

/**
 * Request to put and get Array Data.
 * The data is either taken from or put in the PVArray returned by ChannelArrayRequester.channelArrayConnect.
 */
class epicsShareClass ChannelArray : public ChannelRequest {
public:
    POINTER_DEFINITIONS(ChannelArray);

    /**
     * put to the remote array.
     * @param putArray array to put.
     * @param offset The offset in the remote array, i.e. the PVArray returned by ChannelArrayRequester.channelArrayConnect.
     * @param count The number of elements to put, 0 means "entire array".
     * @param stride 1 means all the elements from offset to count, 2 means every other, 3 means every third, etc.
     */
    virtual void putArray(
        epics::pvData::PVArray::shared_pointer const & putArray,
        size_t offset = 0, size_t count = 0, size_t stride = 1) = 0;

    /**
     * get from the remote array.
     * @param offset The offset in the remote array, i.e. the PVArray returned by ChannelArrayRequester.channelArrayConnect.
     * @param count The number of elements to get, 0 means "till the end of an array".
     * @param stride 1 means all the elements from offset to count, 2 means every other, 3 means every third, etc.
     */
    virtual void getArray(size_t offset = 0, size_t count = 0, size_t stride = 1) = 0;

    /**
     * Get the length.
     */
    virtual void getLength() = 0;

    /**
     * Set the length and/or the capacity.
     * @param length The new length.
     */
    virtual void setLength(size_t length) = 0;
};

/**
 * The epics::pvData::Requester for a ChannelArray.
 */
class epicsShareClass ChannelArrayRequester : virtual public epics::pvData::Requester {
public:
    POINTER_DEFINITIONS(ChannelArrayRequester);

    /**
     * The client and server have both completed the createChannelArray request.
     * @param status Completion status.
     * @param channelArray The channelArray interface or <code>null</code> if the request failed.
     * @param pvArray The PVArray that holds the data or <code>null</code> if the request failed.
     */
    virtual void channelArrayConnect(
        const epics::pvData::Status& status,
        ChannelArray::shared_pointer const & channelArray,
        epics::pvData::Array::const_shared_pointer const & array) = 0;

    /**
     * The request is done. This is always called with no locks held.
     * @param status Completion status.
     * @param channelArray The channelArray interface.
     */
    virtual void putArrayDone(
        const epics::pvData::Status& status,
        ChannelArray::shared_pointer const & channelArray) = 0;

    /**
     * The request is done. This is always called with no locks held.
     * @param status Completion status.
     * @param channelArray The channelArray interface.
     * @param pvArray The PVArray that holds the data or <code>null</code> if the request failed.
     */
    virtual void getArrayDone(
        const epics::pvData::Status& status,
        ChannelArray::shared_pointer const & channelArray,
        epics::pvData::PVArray::shared_pointer const & pvArray) = 0;

    /**
     * The request is done. This is always called with no locks held.
     * @param status Completion status.
     * @param channelArray The channelArray interface.
     * @param length The length of the array, 0 if the request failed.
     */
    virtual void getLengthDone(
        const epics::pvData::Status& status,
        ChannelArray::shared_pointer const & channelArray,
        size_t length) = 0;

    /**
     * The request is done. This is always called with no locks held.
     * @param status Completion status.
     * @param channelArray The channelArray interface.
     */
    virtual void setLengthDone(
        const epics::pvData::Status& status,
        ChannelArray::shared_pointer const & channelArray) = 0;
};


/**
 *
 */
class epicsShareClass ChannelFind : public epics::pvData::Destroyable, private epics::pvData::NoDefaultMethods {
public:
    POINTER_DEFINITIONS(ChannelFind);

    virtual std::tr1::shared_ptr<ChannelProvider> getChannelProvider() = 0;
    virtual void cancel() = 0;
};

/**
 *
 */
class epicsShareClass ChannelFindRequester {
public:
    POINTER_DEFINITIONS(ChannelFindRequester);

    virtual ~ChannelFindRequester() {};

    /**
     * @param status Completion status.
     */
    virtual void channelFindResult(
        const epics::pvData::Status& status,
        ChannelFind::shared_pointer const & channelFind,
        bool wasFound) = 0;
};

/**
 *
 */
class epicsShareClass ChannelListRequester {
public:
    POINTER_DEFINITIONS(ChannelListRequester);

    virtual ~ChannelListRequester() {};

    /**
     * @param status Completion status.
     */
    virtual void channelListResult(
        const epics::pvData::Status& status,
        ChannelFind::shared_pointer const & channelFind,
        epics::pvData::PVStringArray::const_svector const & channelNames,
        bool hasDynamic) = 0;
};

/**
 * Request to get data from a channel.
 */
class epicsShareClass ChannelGet : public ChannelRequest {
public:
    POINTER_DEFINITIONS(ChannelGet);

    /**
     * Get data from the channel.
     * Completion status is reported by calling ChannelGetRequester.getDone() callback.
     */
    virtual void get() = 0;
};


/**
 * epics::pvData::Requester for channelGet.
 */
class epicsShareClass ChannelGetRequester : virtual public epics::pvData::Requester {
public:
    POINTER_DEFINITIONS(ChannelGetRequester);

    /**
     * The client and server have both completed the createChannelGet request.
     * @param status Completion status.
     * @param channelGet The channelGet interface or <code>null</code> if the request failed.
     * @param structure The introspection interface of requested get structure or <code>null</code> if the request failed.
     */
    virtual void channelGetConnect(
        const epics::pvData::Status& status,
        ChannelGet::shared_pointer const & channelGet,
        epics::pvData::Structure::const_shared_pointer const & structure) = 0;

    /**
     * The request is done. This is always called with no locks held.
     * @param status Completion status.
     * @param channelGet The channelGet interface.
     * @param pvStructure The PVStructure that holds the data or <code>null</code> if the request failed.
     * @param bitSet The bitSet for that shows what data has changed or <code>null</code> if the request failed.
     */
    virtual void getDone(
        const epics::pvData::Status& status,
        ChannelGet::shared_pointer const & channelGet,
        epics::pvData::PVStructure::shared_pointer const & pvStructure,
        epics::pvData::BitSet::shared_pointer const & bitSet) = 0;
};


/**
 * ChannelProcess - request that a channel be processed..
 */
class epicsShareClass ChannelProcess : public ChannelRequest {
public:
    POINTER_DEFINITIONS(ChannelProcess);

    /**
     * Issue a process request.
     * Completion status is reported by calling ChannelProcessRequester.processDone() callback.
     */
    virtual void process() = 0;
};


/**
 * epics::pvData::Requester for channelProcess.
 */
class epicsShareClass ChannelProcessRequester : virtual public epics::pvData::Requester {
public:
    POINTER_DEFINITIONS(ChannelProcessRequester);

    /**
     * The client and server have both completed the createChannelProcess request.
     * @param status Completion status.
     * @param channelProcess The channelProcess interface or <code>null</code> if the client could not become
     * the record processor.
     */
    virtual void channelProcessConnect(
        const epics::pvData::Status& status,
        ChannelProcess::shared_pointer const & channelProcess) = 0;

    /**
     * The process request is done. This is always called with no locks held.
     * @param status Completion status.
     * @param channelProcess The channelProcess interface.
     */
    virtual void processDone(
        const epics::pvData::Status& status,
        ChannelProcess::shared_pointer const & channelProcess) = 0;
};


/**
 * Interface for a channel access put request.
 */
class epicsShareClass ChannelPut : public ChannelRequest {
public:
    POINTER_DEFINITIONS(ChannelPut);

    /**
     * Put data to a channel.
     * Completion status is reported by calling ChannelPutRequester.putDone() callback.
     * @param pvPutStructure The PVStructure that holds the putData.
     * @param putBitSet putPVStructure bit-set (selects what fields to put).
     */
    virtual void put(
        epics::pvData::PVStructure::shared_pointer const & pvPutStructure,
        epics::pvData::BitSet::shared_pointer const & putBitSet) = 0;

    /**
     * Get the current data.
     */
    virtual void get() = 0;

};

/**
 * epics::pvData::Requester for ChannelPut.
 */
class epicsShareClass ChannelPutRequester : virtual public epics::pvData::Requester {
public:
    POINTER_DEFINITIONS(ChannelPutRequester);

    /**
     * The client and server have both processed the createChannelPut request.
     * @param status Completion status.
     * @param channelPut The channelPut interface or null if the request failed.
     * @param structure The introspection interface of requested put/get structure or <code>null</code> if the request failed.
     */
    virtual void channelPutConnect(
        const epics::pvData::Status& status,
        ChannelPut::shared_pointer const & channelPut,
        epics::pvData::Structure::const_shared_pointer const & structure) = 0;

    /**
     * The request is done. This is always called with no locks held.
     * @param status Completion status.
     * @param channelPut The channelPut interface.
     */
    virtual void putDone(
        const epics::pvData::Status& status,
        ChannelPut::shared_pointer const & channelPut) = 0;

    /**
     * The get request is done. This is always called with no locks held.
     * @param status Completion status.
     * @param channelPut The channelPut interface.
     * @param pvStructure The PVStructure that holds the data or <code>null</code> if the request failed.
     * @param bitSet The bitSet for that shows what data has changed or <code>null</code> if the request failed.
     */
    virtual void getDone(
        const epics::pvData::Status& status,
        ChannelPut::shared_pointer const & channelPut,
        epics::pvData::PVStructure::shared_pointer const & pvStructure,
        epics::pvData::BitSet::shared_pointer const & bitSet) = 0;
};


/**
 * Channel access put/get request.
 * The put is performed first, followed optionally by a process request, and then by a get request.
 */
class epicsShareClass ChannelPutGet : public ChannelRequest {
public:
    POINTER_DEFINITIONS(ChannelPutGet);

    /**
     * Issue a put/get request. If process was requested when the ChannelPutGet was created this is a put, process, get.
     * Completion status is reported by calling ChannelPutGetRequester.putGetDone() callback.
     * @param pvPutStructure The PVStructure that holds the putData.
     * @param putBitSet putPVStructure bit-set (selects what fields to put).
     */
    virtual void putGet(
        epics::pvData::PVStructure::shared_pointer const & pvPutStructure,
        epics::pvData::BitSet::shared_pointer const & putBitSet) = 0;

    /**
     * Get the put PVStructure. The record will not be processed.
     * Completion status is reported by calling ChannelPutGetRequester.getPutDone() callback.
     */
    virtual void getPut() = 0;

    /**
     * Get the get PVStructure. The record will not be processed.
     * Completion status is reported by calling ChannelPutGetRequester.getGetDone() callback.
     */
    virtual void getGet() = 0;
};


/**
 * epics::pvData::Requester for ChannelPutGet.
 */
class epicsShareClass ChannelPutGetRequester : virtual public epics::pvData::Requester
{
public:
    POINTER_DEFINITIONS(ChannelPutGetRequester);

    /**
     * The client and server have both completed the createChannelPutGet request.
     * @param status Completion status.
     * @param channelPutGet The channelPutGet interface or null if the request failed.
     * @param putStructure The put structure introspection data or <code>null</code> if the request failed.
     * @param getStructure The get structure introspection data or <code>null</code> if the request failed.
     */
    virtual void channelPutGetConnect(
        const epics::pvData::Status& status,
        ChannelPutGet::shared_pointer const & channelPutGet,
        epics::pvData::Structure::const_shared_pointer const & putStructure,
        epics::pvData::Structure::const_shared_pointer const & getStructure) = 0;

    /**
     * The putGet request is done. This is always called with no locks held.
     * @param status Completion status.
     * @param channelPutGet The channelPutGet interface.
     * @param pvGetStructure The PVStructure that holds the getData or <code>null</code> if the request failed.
     * @param getBitSet getPVStructure changed bit-set or <code>null</code> if the request failed.
     */
    virtual void putGetDone(
        const epics::pvData::Status& status,
        ChannelPutGet::shared_pointer const & channelPutGet,
        epics::pvData::PVStructure::shared_pointer const & pvGetStructure,
        epics::pvData::BitSet::shared_pointer const & getBitSet) = 0;

    /**
     * The getPut request is done. This is always called with no locks held.
     * @param status Completion status.
     * @param channelPutGet The channelPutGet interface.
     * @param pvPutStructure The PVStructure that holds the putData or <code>null</code> if the request failed.
     * @param putBitSet putPVStructure changed bit-set or <code>null</code> if the request failed.
     */
    virtual void getPutDone(
        const epics::pvData::Status& status,
        ChannelPutGet::shared_pointer const & channelPutGet,
        epics::pvData::PVStructure::shared_pointer const & pvPutStructure,
        epics::pvData::BitSet::shared_pointer const & putBitSet) = 0;

    /**
     * The getGet request is done. This is always called with no locks held.
     * @param status Completion status.
     * @param channelPutGet The channelPutGet interface.
     * @param pvGetStructure The PVStructure that holds the getData or <code>null</code> if the request failed.
     * @param getBitSet getPVStructure changed bit-set or <code>null</code> if the request failed.
     */
    virtual void getGetDone(
        const epics::pvData::Status& status,
        ChannelPutGet::shared_pointer const & channelPutGet,
        epics::pvData::PVStructure::shared_pointer const & pvGetStructure,
        epics::pvData::BitSet::shared_pointer const & getBitSet) = 0;
};


/**
 * epics::pvData::Requester for channelGet.
 */
class epicsShareClass ChannelRPC : public ChannelRequest {
public:
    POINTER_DEFINITIONS(ChannelRPC);

    /**
     * Issue an RPC request to the channel.
     * Completion status is reported by calling ChannelRPCRequester.requestDone() callback.
     * @param pvArgument The argument structure for an RPC request.
     */
    virtual void request(epics::pvData::PVStructure::shared_pointer const & pvArgument) = 0;
};


/**
 * epics::pvData::Requester for channelGet.
 */
class epicsShareClass ChannelRPCRequester : virtual public epics::pvData::Requester {
public:
    POINTER_DEFINITIONS(ChannelRPCRequester);

    /**
     * The client and server have both completed the createChannelGet request.
     * @param status Completion status.
     * @param channelRPC The channelRPC interface or <code>null</code> if the request failed.
     */
    virtual void channelRPCConnect(
        const epics::pvData::Status& status,
        ChannelRPC::shared_pointer const & channelRPC) = 0;

    /**
     * The request is done. This is always called with no locks held.
     * @param status Completion status.
     * @param channelRPC The channelRPC interface.
     * @param pvResponse The response data for the RPC request or <code>null</code> if the request failed.
     */
    virtual void requestDone(
        const epics::pvData::Status& status,
        ChannelRPC::shared_pointer const & channelRPC,
        epics::pvData::PVStructure::shared_pointer const & pvResponse) = 0;
};


/**
 * epics::pvData::Requester for a getStructure request.
 */
class epicsShareClass GetFieldRequester : virtual public epics::pvData::Requester {
public:
    POINTER_DEFINITIONS(GetFieldRequester);

    /**
     * The client and server have both completed the getStructure request.
     * @param status Completion status.
     * @param field The Structure for the request.
     */
    virtual void getDone(
        const epics::pvData::Status& status,
        epics::pvData::FieldConstPtr const & field) = 0;     // TODO naming convention

};


class ChannelRequester;

/**
 * Interface for accessing a channel.
 * A channel is created via a call to ChannelAccess.createChannel(std::string channelName).
 */
class epicsShareClass Channel :
    public epics::pvData::Requester,
    public epics::pvData::Destroyable,
    private epics::pvData::NoDefaultMethods {
public:
    POINTER_DEFINITIONS(Channel);

    virtual ~Channel() {}

    virtual std::string getRequesterName();
    virtual void message(std::string const & message, epics::pvData::MessageType messageType);

    /**
     * Channel connection status.
     */
    enum ConnectionState {
        NEVER_CONNECTED, CONNECTED, DISCONNECTED, DESTROYED
    };

    static const char* ConnectionStateNames[];

    /**
     * Get the the channel provider of this channel.
     * @return The channel provider.
     */
    virtual std::tr1::shared_ptr<ChannelProvider> getProvider() = 0;
//            virtual ChannelProvider::shared_pointer getProvider() = 0;

    /**
     * Returns the channel's remote address, signal name, etc...
     * For example:
     *     - client side channel would return server's address, e.g. "/192.168.1.101:5064"
     *     - server side channel would return underlying bus address, e.g. "#C0 S1".
     * @return the channel's address.
     **/
    virtual std::string getRemoteAddress() = 0;

    /**
     * Returns the connection state of this channel.
     * @return the <code>ConnectionState</code> value.
     **/
    virtual ConnectionState getConnectionState() = 0;

    /**
     * Get the channel name.
     * @return The name.
     */
    virtual std::string getChannelName() = 0;

    /**
     * Get the channel epics::pvData::Requester.
     * @return The epics::pvData::Requester.
     */
//            virtual ChannelRequester::shared_pointer getChannelRequester() = 0;
    virtual std::tr1::shared_ptr<ChannelRequester> getChannelRequester() = 0;

    /**
     * Is the channel connected?
     * @return (false,true) means (not, is) connected.
     */
    virtual bool isConnected() { return getConnectionState()==CONNECTED; }

    /**
     * Get a Field which describes the subField.
     * GetFieldRequester.getDone is called after both client and server have processed the getField request.
     * This is for clients that want to introspect a PVRecord via channel access.
     * @param epics::pvData::Requester The epics::pvData::Requester.
     * @param subField The name of the subField.
     * If this is null or an empty std::string the returned Field is for the entire record.
     */
    virtual void getField(GetFieldRequester::shared_pointer const & requester,std::string const & subField) = 0;

    /**
     * Get the access rights for a field of a PVStructure created via a call to createPVStructure.
     * MATEJ Channel access can store this info via auxInfo.
     * @param pvField The field for which access rights is desired.
     * @return The access rights.
     */
    virtual AccessRights getAccessRights(epics::pvData::PVField::shared_pointer const & pvField) = 0;

    /**
     * Create a ChannelProcess.
     * ChannelProcessRequester.channelProcessReady is called after both client and server are ready for
     * the client to make a process request.
     * @param channelProcessRequester The interface for notifying when this request is complete
     * and when channel completes processing.
     * @param pvRequest Additional options (e.g. triggering).
     * @return <code>ChannelProcess</code> instance.
     */
    virtual ChannelProcess::shared_pointer createChannelProcess(
        ChannelProcessRequester::shared_pointer const & channelProcessRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    /**
     * Create a ChannelGet.
     * ChannelGetRequester.channelGetReady is called after both client and server are ready for
     * the client to make a get request.
     * @param channelGetRequester The interface for notifying when this request is complete
     * and when a channel get completes.
     * @param pvRequest A structure describing the desired set of fields from the remote PVRecord.
     * This has the same form as a pvRequest to PVCopyFactory.create.
     * @return <code>ChannelGet</code> instance.
     */
    virtual ChannelGet::shared_pointer createChannelGet(
        ChannelGetRequester::shared_pointer const & channelGetRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    /**
     * Create a ChannelPut.
     * ChannelPutRequester.channelPutReady is called after both client and server are ready for
     * the client to make a put request.
     * @param channelPutRequester The interface for notifying when this request is complete
     * and when a channel get completes.
     * @param pvRequest A structure describing the desired set of fields from the remote PVRecord.
     * This has the same form as a pvRequest to PVCopyFactory.create.
     * @return <code>ChannelPut</code> instance.
     */
    virtual ChannelPut::shared_pointer createChannelPut(
        ChannelPutRequester::shared_pointer const & channelPutRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    /**
     * Create a ChannelPutGet.
     * ChannelPutGetRequester.channelPutGetReady is called after both client and server are ready for
     * the client to make a putGet request.
     * @param channelPutGetRequester The interface for notifying when this request is complete
     * and when a channel get completes.
     * @param pvRequest A structure describing the desired set of fields from the remote PVRecord.
     * This has the same form as a pvRequest to PVCopyFactory.create.
     * @return <code>ChannelPutGet</code> instance.
     */
    virtual ChannelPutGet::shared_pointer createChannelPutGet(
        ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    /**
     * Create a ChannelRPC (Remote Procedure Call).
     * @param channelRPCRequester The epics::pvData::Requester.
     * @param pvRequest Request options.
     * @return <code>ChannelRPC</code> instance.
     */
    virtual ChannelRPC::shared_pointer createChannelRPC(
        ChannelRPCRequester::shared_pointer const & channelRPCRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    /**
     * Create a Monitor.
     * @param monitorRequester The epics::pvData::Requester.
     * @param pvRequest A structure describing the desired set of fields from the remote PVRecord.
     * This has the same form as a pvRequest to PVCopyFactory.create.
     * @return <code>Monitor</code> instance.
     */
    virtual epics::pvData::Monitor::shared_pointer createMonitor(
        epics::pvData::MonitorRequester::shared_pointer const & monitorRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    /**
     * Create a ChannelArray.
     * @param channelArrayRequester The ChannelArrayRequester
     * @param pvRequest Additional options (e.g. triggering).
     * @return <code>ChannelArray</code> instance.
     */
    virtual ChannelArray::shared_pointer createChannelArray(
        ChannelArrayRequester::shared_pointer const & channelArrayRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    /**
     * Prints detailed information about the context to the standard output stream.
     */
    virtual void printInfo() { printInfo(std::cout); }

    /**
     * Prints detailed information about the context to the specified output stream.
     * @param out the output stream.
     */
    virtual void printInfo(std::ostream& out) {}
};


/**
 * Listener for connect state changes.
 */
class epicsShareClass ChannelRequester : public virtual epics::pvData::Requester {
public:
    POINTER_DEFINITIONS(ChannelRequester);

    /**
     * A channel has been created. This may be called multiple times if there are multiple providers.
     * @param status Completion status.
     * @param channel The channel.
     */
    virtual void channelCreated(const epics::pvData::Status& status, Channel::shared_pointer const & channel) = 0;

    /**
     * A channel connection state change has occurred.
     * @param c The channel.
     * @param connectionState The new connection state.
     */
    virtual void channelStateChange(Channel::shared_pointer const & channel, Channel::ConnectionState connectionState) = 0;
};

/**
 * @brief The FlushStrategy enum
 */
enum FlushStrategy {
    IMMEDIATE, DELAYED, USER_CONTROLED
};

/**
 * Interface implemented by code that can provide access to the record
 * to which a channel connects.
 */
class epicsShareClass ChannelProvider : public epics::pvData::Destroyable, private epics::pvData::NoDefaultMethods {
public:
    POINTER_DEFINITIONS(ChannelProvider);

    /** Minimal priority. */
    static const short PRIORITY_MIN = 0;
    /** Maximal priority. */
    static const short PRIORITY_MAX = 99;
    /** Default priority. */
    static const short PRIORITY_DEFAULT = PRIORITY_MIN;
    /** DB links priority. */
    static const short PRIORITY_LINKS_DB = PRIORITY_MAX;
    /** Archive priority. */
    static const short PRIORITY_ARCHIVE = (PRIORITY_MAX + PRIORITY_MIN) / 2;
    /** OPI priority. */
    static const short PRIORITY_OPI = PRIORITY_MIN;

    /**
     * Get the provider name.
     * @return The name.
     */
    virtual std::string getProviderName() = 0;

    /**
     * Find a channel.
     * @param channelName The channel name.
     * @param channelFindRequester The epics::pvData::Requester.
     * @return An interface for the find.
     */
    virtual ChannelFind::shared_pointer channelFind(std::string const & channelName,
            ChannelFindRequester::shared_pointer const & channelFindRequester) = 0;

    /**
     * Find channels.
     * @param channelFindRequester The epics::pvData::Requester.
     * @return An interface for the find.
     */
    virtual ChannelFind::shared_pointer channelList(ChannelListRequester::shared_pointer const & channelListRequester) = 0;

    /**
     * Create a channel.
     * @param channelName The name of the channel.
     * @param channelRequester The epics::pvData::Requester.
     * @param priority channel priority, must be <code>PRIORITY_MIN</code> <= priority <= <code>PRIORITY_MAX</code>.
     * @return <code>Channel</code> instance. If channel does not exist <code>null</code> is returned and <code>channelRequester</code> notified.
     */
    virtual Channel::shared_pointer createChannel(std::string const & channelName,ChannelRequester::shared_pointer const & channelRequester,
            short priority = PRIORITY_DEFAULT) = 0;

    /**
     * Create a channel.
     * @param channelName The name of the channel.
     * @param channelRequester The epics::pvData::Requester.
     * @param priority channel priority, must be <code>PRIORITY_MIN</code> <= priority <= <code>PRIORITY_MAX</code>.
     * @param address address (or list of addresses) where to look for a channel. Implementation independed std::string.
     * @return <code>Channel</code> instance. If channel does not exist <code>null</code> is returned and <code>channelRequester</code> notified.
     */
    virtual Channel::shared_pointer createChannel(std::string const & channelName,ChannelRequester::shared_pointer const & channelRequester,
            short priority, std::string const & address) = 0;

    virtual void configure(epics::pvData::PVStructure::shared_pointer /*configuration*/) EPICS_DEPRECATED {};
    virtual void flush() {};
    virtual void poll() {};

};

/**
 * <code>ChanneProvider</code> factory interface.
 */
class epicsShareClass ChannelProviderFactory : private epics::pvData::NoDefaultMethods {
public:
    POINTER_DEFINITIONS(ChannelProviderFactory);

    virtual ~ChannelProviderFactory() {};

    /**
     * Get factory name (i.e. name of the provider).
     * @return the factory name.
     */
    virtual std::string getFactoryName() = 0;

    /**
     * Get a shared instance using the default Configuration.
     * @return a shared instance.
     */
    virtual ChannelProvider::shared_pointer sharedInstance() = 0;

    /**
     * Create a new instance using the default Configuration.
     * @return a new instance.
     */
    virtual ChannelProvider::shared_pointer newInstance() {
        return newInstance(std::tr1::shared_ptr<Configuration>());
    }

    /**
     * Create a new instance using a specific Configuration.
     * @return a new instance.
     */
    virtual ChannelProvider::shared_pointer newInstance(const std::tr1::shared_ptr<Configuration>&) {
        throw std::logic_error("This ChannelProviderFactory does not support non-default configurations");
    }
};

/**
 * Interface for locating channel providers.
 */
class epicsShareClass ChannelProviderRegistry : private epics::pvData::NoDefaultMethods {
public:
    POINTER_DEFINITIONS(ChannelProviderRegistry);

    typedef std::vector<std::string> stringVector_t;

    virtual ~ChannelProviderRegistry() {};

    /**
     * Get a shared instance of the provider with the specified name.
     * @param providerName The name of the provider.
     * @return The interface for the provider or null if the provider is not known.
     */
    virtual ChannelProvider::shared_pointer getProvider(std::string const & providerName) = 0;

    /**
     * Creates a new instanceof the provider with the specified name.
     * @param providerName The name of the provider.
     * @return The interface for the provider or null if the provider is not known.
     */
    virtual ChannelProvider::shared_pointer createProvider(std::string const & providerName) = 0;

    /**
     * Get a array of the names of all the known providers.
     * @return The names. Be sure to delete vector instance.
     */
    virtual std::auto_ptr<stringVector_t> getProviderNames() = 0;
};

epicsShareExtern ChannelProviderRegistry::shared_pointer getChannelProviderRegistry();
epicsShareExtern void registerChannelProviderFactory(ChannelProviderFactory::shared_pointer const & channelProviderFactory);
epicsShareExtern void unregisterChannelProviderFactory(ChannelProviderFactory::shared_pointer const & channelProviderFactory);


/**
 * @brief Pipeline (streaming) support API (optional).
 * This is used by pvAccess to implement pipeline (streaming) monitors.
 */
class epicsShareClass PipelineMonitor : public virtual epics::pvData::Monitor {
public:
    POINTER_DEFINITIONS(PipelineMonitor);
    virtual ~PipelineMonitor() {}

    /**
     * Report remote queue status.
     * @param freeElements number of free elements.
     */
    virtual void reportRemoteQueueStatus(epics::pvData::int32 freeElements) = 0;
};


}
}

#endif  /* PVACCESS_H */

/** @page Overview Documentation
 *
 *<a href = "../documentation/pvAccessCPP.html">pvAccessCPP.html</a>
 *
 */

