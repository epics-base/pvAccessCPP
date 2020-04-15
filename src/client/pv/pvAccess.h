/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVACCESS_H
#define PVACCESS_H

#include <vector>
#include <set>

#ifdef epicsExportSharedSymbols
#   define pvAccessEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/pvData.h>
#include <pv/createRequest.h>
#include <pv/status.h>
#include <pv/bitSet.h>

#ifdef pvAccessEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#       undef pvAccessEpicsExportSharedSymbols
#endif

#include <pv/pvaVersion.h>
#include <pv/destroyable.h>
#include <pv/monitor.h>

#include <shareLib.h>

/* C++11 keywords
 @code
 struct Base {
   virtual void foo();
 };
 struct Class : public Base {
   virtual void foo() OVERRIDE FINAL;
 };
 @endcode
 */
#ifndef FINAL
#  if __cplusplus>=201103L
#    define FINAL final
#  else
#    define FINAL
#  endif
#endif
#ifndef OVERRIDE
#  if __cplusplus>=201103L
#    define OVERRIDE override
#  else
#    define OVERRIDE
#  endif
#endif

namespace epics {
//! Holds all PVA related
namespace pvAccess {
class Configuration;

using epics::pvData::Requester;
using epics::pvData::RequesterPtr;
using epics::pvData::MessageType;
using epics::pvData::getMessageTypeName;

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

    virtual ~Lockable() {}

    virtual void lock() {}
    virtual void unlock() {}
};

/**
 * Scope lock.
 */
class epicsShareClass ScopedLock {
    EPICS_NOT_COPYABLE(ScopedLock)
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
class ChannelArrayRequester;
class ChannelFindRequester;
class ChannelGetRequester;
class ChannelProcessRequester;
class ChannelPutRequester;
class ChannelPutGetRequester;
class ChannelRPCRequester;

/** @brief Expose statistics related to network transport
 *
 * Various sub-classes of ChannelBaseRequester (for servers) or ChannelRequest (for clients)
 * may by dynamic_cast<>able to NetStats.
 */
struct epicsShareClass NetStats {
    struct Counter {
        size_t tx, rx;

        inline Counter() :tx(0u), rx(0u) {}
    };
    struct Stats {
        std::string transportPeer;
        Counter transportBytes;
        Counter operationBytes;
        bool populated;

        inline Stats() :populated(false) {}
    };

    virtual ~NetStats();
    //! Query current counter values
    virtual void stats(Stats& s) const =0;
};

//! Base for all Requesters (callbacks to client)
struct epicsShareClass ChannelBaseRequester : virtual public epics::pvData::Requester
{
    POINTER_DEFINITIONS(ChannelBaseRequester);

    static size_t num_instances;

    ChannelBaseRequester();
    virtual ~ChannelBaseRequester();

    /** Notification when underlying Channel becomes DISCONNECTED or DESTORYED
     *
     * (re)connection is notified through a sub-class *Connect() method.
     *
     * Any in-progress get()/put()/request()/start() is implicitly cancel()'d or stop()'d
     * before this method is called.
     *
     * Ownership of any PVStructures passed to completion callbacks (eg. ChannelGetRequester::getDone() )
     * is returned the operation
     *
     * @param destroy true for final disconnect.
     */
    virtual void channelDisconnect(bool destroy) {}

private:
    ChannelBaseRequester(const ChannelBaseRequester&);
    ChannelBaseRequester& operator=(const ChannelBaseRequester&);
};

/**
 * Base interface for all channel requests (aka. Operations).
 */
class epicsShareClass ChannelRequest : public virtual Destroyable, public Lockable {
public:
    POINTER_DEFINITIONS(ChannelRequest);

    static size_t num_instances;

    ChannelRequest();
    virtual ~ChannelRequest();

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

private:
    EPICS_NOT_COPYABLE(ChannelRequest)
};

/**
 * @brief Callback implemented by monitor clients.
 *
 * Requester for ChannelMonitor.
 * @author mrk
 */
class epicsShareClass MonitorRequester : public ChannelBaseRequester {
    public:
    POINTER_DEFINITIONS(MonitorRequester);
    typedef Monitor operation_type;

    virtual ~MonitorRequester(){}
    /**
     * Underlying Channel is connected and operation setup is complete.
     * Call start() to begin subscription updates.
     *
     * @param status Completion status.
     * @param monitor The monitor
     * @param structure The structure defining the data.
     */
    virtual void monitorConnect(epics::pvData::Status const & status,
        MonitorPtr const & monitor, epics::pvData::StructureConstPtr const & structure) = 0;
    /**
     * Monitor queue is not empty.
     *
     * The requester must call Monitor.poll to get data.
     * @param monitor The monitor.
     */
    virtual void monitorEvent(MonitorPtr const & monitor) = 0;
    /**
     * No more subscription update will be sent.
     * @param monitor The monitor.
     */
    virtual void unlisten(MonitorPtr const & monitor) = 0;
};

/**
 * Request to put and get Array Data.
 * The data is either taken from or put in the PVArray returned by ChannelArrayRequester.channelArrayConnect.
 */
class epicsShareClass ChannelArray : public ChannelRequest {
public:
    POINTER_DEFINITIONS(ChannelArray);
    typedef ChannelArrayRequester requester_type;

    virtual ~ChannelArray() {}

    /**
     * put to the remote array.
     *
     * Ownership of the PVArray is transferred to the ChannelArray until ChannelArrayRequester::putArrayDone()
     * or ChannelArrayRequester::channelDisconnect() is called.
     *
     * @param putArray array to put.
     * @param offset The offset in the remote array, i.e. the PVArray returned by ChannelArrayRequester::channelArrayConnect.
     * @param count The number of elements to put, 0 means "entire array".
     * @param stride 1 means all the elements from offset to count, 2 means every other, 3 means every third, etc.
     */
    virtual void putArray(
        epics::pvData::PVArray::shared_pointer const & putArray,
        size_t offset = 0, size_t count = 0, size_t stride = 1) = 0;

    /**
     * get from the remote array.
     *
     * Ownership of the PVArray previously passed to ChannelArrayRequester::getArrayDone()
     * is returned to the ChannelArray from the ChannelArrayRequester.
     *
     * @param offset The offset in the remote array, i.e. the PVArray returned by ChannelArrayRequester::channelArrayConnect.
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
 * The Requester for a ChannelArray.
 */
class epicsShareClass ChannelArrayRequester : public ChannelBaseRequester {
public:
    POINTER_DEFINITIONS(ChannelArrayRequester);
    typedef ChannelArray operation_type;

    virtual ~ChannelArrayRequester() {}

    /**
     * Underlying Channel is connected and operation setup is complete.
     * May call putArray(), getArray(), getLength(), or setLength() to execute.
     *
     * @param status Completion status.
     * @param channelArray The channelArray interface or nullptr if the request failed.
     * @param pvArray The PVArray that holds the data or nullptr if the request failed.
     */
    virtual void channelArrayConnect(
        const epics::pvData::Status& status,
        ChannelArray::shared_pointer const & channelArray,
        epics::pvData::Array::const_shared_pointer const & array) = 0;

    /**
     * The request is done. This is always called with no locks held.
     *
     * Ownership of PVArray passed to ChannelArray::putArray() returns to ChannelArrayRequester
     *
     * @param status Completion status.
     * @param channelArray The channelArray interface.
     */
    virtual void putArrayDone(
        const epics::pvData::Status& status,
        ChannelArray::shared_pointer const & channelArray) = 0;

    /**
     * The request is done. This is always called with no locks held.
     *
     * Ownership of the PVArray is transfered to the ChannelArrayRequester until
     * a subsequent call to ChannelArray::getArray() or ChannelArrayRequester::channelDisconnect().
     *
     * @param status Completion status.
     * @param channelArray The channelArray interface.
     * @param pvArray The PVArray that holds the data or nullptr if the request failed.
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
class epicsShareClass ChannelFind : public Destroyable {
    EPICS_NOT_COPYABLE(ChannelFind)
public:
    POINTER_DEFINITIONS(ChannelFind);
    typedef ChannelFindRequester requester_type;

    ChannelFind() {}
    virtual ~ChannelFind() {}

    virtual std::tr1::shared_ptr<ChannelProvider> getChannelProvider() = 0;
    virtual void cancel() = 0;

    //! Allocate a no-op ChannelFind.  This is sufficient for most, if not all, ChannelProvider implementations.
    //! Holds only a weak_ptr<ChannelProvider>
    static ChannelFind::shared_pointer buildDummy(const std::tr1::shared_ptr<ChannelProvider>& provider);
};

struct PeerInfo; // see pv/security.h

/**
 *
 */
class epicsShareClass ChannelFindRequester {
public:
    POINTER_DEFINITIONS(ChannelFindRequester);
    typedef ChannelFind operation_type;

    virtual ~ChannelFindRequester() {}

    /**
     * @param status Completion status.
     */
    virtual void channelFindResult(
        const epics::pvData::Status& status,
        ChannelFind::shared_pointer const & channelFind,
        bool wasFound) = 0;

    /**
     * @brief Return information on requesting peer if applicable.
     *
     * A server-type ChannelProvider will use this method to discover if a remote client
     * has provided credentials which may be used in access control decisions.
     *
     * The returned PeerInfo is only required to have valid values for: peer, transport, and transportVersion.
     * PeerInfo::authority may be empty if authentication has not yet occured (UDP search).
     *
     * Default implementation returns NULL.
     *
     * isConnected()==true and getPeerInfo()==NULL when the ChannelProvider does not provide
     * information about the peer.  This should be treated as an unauthenticated, anonymous,
     * peer.
     *
     * The returned instance must not change, and a different instance should be returned
     * if/when peer information changes (eg. after reconnect).
     *
     * May return !NULL when !isConnected().  getPeerInfo() must be called _before_
     * testing isConnected() in situations where connection state is being polled.
     */
    virtual std::tr1::shared_ptr<const PeerInfo> getPeerInfo()
    { return std::tr1::shared_ptr<const PeerInfo>(); }
};

/**
 *
 */
class epicsShareClass ChannelListRequester {
public:
    POINTER_DEFINITIONS(ChannelListRequester);
    typedef ChannelFind operation_type;

    virtual ~ChannelListRequester() {}

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
    typedef ChannelGetRequester requester_type;

    virtual ~ChannelGet() {}

    /**
     * Get data from the channel.
     *
     * Ownership of the PVStructure passed to ChannelGetRequester::getDone() is
     * returned to the ChannelGet.
     *
     * Completion status is reported by calling ChannelGetRequester::getDone() callback.
     */
    virtual void get() = 0;
};


/**
 * Requester for channelGet.
 */
class epicsShareClass ChannelGetRequester : public ChannelBaseRequester {
public:
    POINTER_DEFINITIONS(ChannelGetRequester);
    typedef ChannelGet operation_type;

    virtual ~ChannelGetRequester() {}

    /**
     * The client and server have both completed the createChannelGet request.
     * @param status Completion status.
     * @param channelGet The channelGet interface or nullptr if the request failed.
     * @param structure The introspection interface of requested get structure or nullptr if the request failed.
     */
    virtual void channelGetConnect(
        const epics::pvData::Status& status,
        ChannelGet::shared_pointer const & channelGet,
        epics::pvData::Structure::const_shared_pointer const & structure) = 0;

    /**
     * The request is done. This is always called with no locks held.
     *
     * Ownership of the PVStructure is passed to the ChannelGetRequester until a subsequent call to
     * ChannelGet::get() or ChannelGetRequester::channelDisconnect()
     *
     * @param status Completion status.
     * @param channelGet The channelGet interface.
     * @param pvStructure The PVStructure that holds the data or nullptr if the request failed.
     * @param bitSet The bitSet for that shows what data has changed or nullptr if the request failed.
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
    typedef ChannelProcessRequester requester_type;

    virtual ~ChannelProcess() {}

    /**
     * Issue a process request.
     * Completion status is reported by calling ChannelProcessRequester.processDone() callback.
     */
    virtual void process() = 0;
};


/**
 * Requester for channelProcess.
 */
class epicsShareClass ChannelProcessRequester : public ChannelBaseRequester {
public:
    POINTER_DEFINITIONS(ChannelProcessRequester);
    typedef ChannelProcess operation_type;

    virtual ~ChannelProcessRequester() {}

    /**
     * The client and server have both completed the createChannelProcess request.
     * @param status Completion status.
     * @param channelProcess The channelProcess interface or nullptr if the client could not become
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
    typedef ChannelPutRequester requester_type;

    virtual ~ChannelPut() {}

    /**
     * Put data to a channel.
     *
     * Completion status is reported by calling ChannelPutRequester::putDone()
     *
     * Ownership of the PVStructure is transfered to the ChannelPut until
     * ChannelPutRequester::putDone() or ChannelPutRequester::channelDisconnect()
     * is called.
     *
     * @param pvPutStructure The PVStructure that holds the putData.
     * @param putBitSet putPVStructure bit-set (selects what fields to put).
     */
    virtual void put(
        epics::pvData::PVStructure::shared_pointer const & pvPutStructure,
        epics::pvData::BitSet::shared_pointer const & putBitSet) = 0;

    /**
     * Get the current data.
     *
     * Ownership transfer as with ChannelGet::get()
     */
    virtual void get() = 0;

};

/**
 * Requester for ChannelPut.
 */
class epicsShareClass ChannelPutRequester : public ChannelBaseRequester {
public:
    POINTER_DEFINITIONS(ChannelPutRequester);
    typedef ChannelPut operation_type;

    virtual ~ChannelPutRequester() {}

    /**
     * The client and server have both processed the createChannelPut request.
     * @param status Completion status.
     * @param channelPut The channelPut interface or null if the request failed.
     * @param structure The introspection interface of requested put/get structure or nullptr if the request failed.
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
     *
     * Ownership transfer as with ChannelGetRequester::getDone()
     *
     * @param status Completion status.
     * @param channelPut The channelPut interface.
     * @param pvStructure The PVStructure that holds the data or nullptr if the request failed.
     * @param bitSet The bitSet for that shows what data has changed or nullptr if the request failed.
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
    typedef ChannelPutGetRequester requester_type;

    virtual ~ChannelPutGet() {}

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
 * Requester for ChannelPutGet.
 */
class epicsShareClass ChannelPutGetRequester : public ChannelBaseRequester
{
public:
    POINTER_DEFINITIONS(ChannelPutGetRequester);
    typedef ChannelPutGet operation_type;

    virtual ~ChannelPutGetRequester() {}

    /**
     * The client and server have both completed the createChannelPutGet request.
     * @param status Completion status.
     * @param channelPutGet The channelPutGet interface or null if the request failed.
     * @param putStructure The put structure introspection data or nullptr if the request failed.
     * @param getStructure The get structure introspection data or nullptr if the request failed.
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
     * @param pvGetStructure The PVStructure that holds the getData or nullptr if the request failed.
     * @param getBitSet getPVStructure changed bit-set or nullptr if the request failed.
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
     * @param pvPutStructure The PVStructure that holds the putData or nullptr if the request failed.
     * @param putBitSet putPVStructure changed bit-set or nullptr if the request failed.
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
     * @param pvGetStructure The PVStructure that holds the getData or nullptr if the request failed.
     * @param getBitSet getPVStructure changed bit-set or nullptr if the request failed.
     */
    virtual void getGetDone(
        const epics::pvData::Status& status,
        ChannelPutGet::shared_pointer const & channelPutGet,
        epics::pvData::PVStructure::shared_pointer const & pvGetStructure,
        epics::pvData::BitSet::shared_pointer const & getBitSet) = 0;
};


/**
 * Handle for an RPC operation
 */
class epicsShareClass ChannelRPC : public ChannelRequest {
public:
    POINTER_DEFINITIONS(ChannelRPC);
    typedef ChannelRPCRequester requester_type;

    virtual ~ChannelRPC() {}

    /**
     * Issue an RPC request to the channel.
     *
     * Completion status is reported by calling ChannelRPCRequester::requestDone() callback,
     * which may be called from this method.
     *
     * @pre The underlying Channel must be connected, and this ChannelRPC valid.
     *      Otherwise the ChannelRPCRequester::requestDone() is called with an error.
     *
     * @post After calling request(), the requestDone() callback will be called at some later time.
     *       May call ChannelRPC::cancel() to request to abort() this operation.
     *
     * @param pvArgument The argument structure for an RPC request.
     */
    virtual void request(epics::pvData::PVStructure::shared_pointer const & pvArgument) = 0;
};


/**
 * Notifications associated with Channel::createChannelRPC()
 *
 * No locks may be held while calling these methods.
 */
class epicsShareClass ChannelRPCRequester : public ChannelBaseRequester {
public:
    POINTER_DEFINITIONS(ChannelRPCRequester);
    typedef ChannelRPC operation_type;

    virtual ~ChannelRPCRequester() {}

    /**
     * RPC creation request satisfied.
     *
     * Must check status.isOk().
     *
     * On Success, a non-NULL 'operation' is provided.
     * This is the same pointer which was, or will be, returned from Channel::createChannelRPC().
     *
     * It is allowed to call ChannelRPC::request() from within this method.
     */
    virtual void channelRPCConnect(
        const epics::pvData::Status& status,
        ChannelRPC::shared_pointer const & operation) = 0;

    /**
     * RPC request (execution) completed.
     *
     * Must check status.isOk().
     *
     * On Success, a non-NULL 'pvResponse' is provided.
     *
     * It is allowed to call ChannelRPC::request() from within this method.
     */
    virtual void requestDone(
        const epics::pvData::Status& status,
        ChannelRPC::shared_pointer const & operation,
        epics::pvData::PVStructure::shared_pointer const & pvResponse) = 0;
};


/**
 * Completion notification for Channel::getField()
 */
class epicsShareClass GetFieldRequester : public ChannelBaseRequester {
public:
    POINTER_DEFINITIONS(GetFieldRequester);

    virtual ~GetFieldRequester() {}

    /**
     * Check status.isOk() to determine success.
     * On success the 'field' will be non-NULL.
     * On failure 'field' will be NULL.
     *
     * @param status Completion status.
     * @param field The Structure for the request.
     */
    virtual void getDone(
        const epics::pvData::Status& status,
        epics::pvData::FieldConstPtr const & field) = 0;     // TODO naming convention

};

class ChannelRequester;

/**
 * The interface through which Operations (get, put, monitor, ...) are initiated.
 *
 * Handle for a Channel returned by ChannelProvider::createChannel()
 *
 * At any given moment a Channel may be CONNECTED or DISCONNECTED.  (NEVER_CONNECTED and DESTORYED are special cases of DISCONNECTED)
 *
 * A Channel is required to honor calls to Channel::create*() methods while in the disconnected state.
 *
 * A Channel is required to maintain a strong reference (shared_ptr<>) to the ChannelProvider through which it was created.
 */
class epicsShareClass Channel :
    public Requester,
    public Destroyable
{
    EPICS_NOT_COPYABLE(Channel)
public:
    POINTER_DEFINITIONS(Channel);
    typedef ChannelRequester requester_type;

    static size_t num_instances;

    Channel();
    virtual ~Channel();

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
     * The ChannelProvider from which this Channel was requested.
     * May never be NULL.
     */
    virtual std::tr1::shared_ptr<ChannelProvider> getProvider() = 0;

    /**
     * Returns the channel's remote address, signal name, etc...
     * For example:
     *     - client side channel would return server's address, e.g. "/192.168.1.101:5064"
     *     - server side channel would return underlying bus address, e.g. "#C0 S1".
     *
     * The value returned here will changed depending on the connection status.
     * A disconnected channel should return an empty() string.
     **/
    virtual std::string getRemoteAddress() = 0;

    /**
     * Poll the connection state in more detail
     **/
    virtual ConnectionState getConnectionState();

    /**
     * The name passed to ChannelProvider::createChannel()
     */
    virtual std::string getChannelName() = 0;

    /**
     * The ChannelRequester passed to ChannelProvider::createChannel()
     *
     * @throws std::tr1::bad_weak_ptr
     */
    virtual std::tr1::shared_ptr<ChannelRequester> getChannelRequester() = 0;

    /**
     * Poll connection state
     */
    virtual bool isConnected();

    /**
     * Initiate a request to retrieve a description of the structure of this Channel.
     *
     * While the type described by calls to getField() should match what is provided for all operations except RPC.
     *
     * GetFieldRequester::getDone() will be called before getField() returns, or at some time afterwards.
     *
     * @param Requester The Requester.
     * @param subField Empty string, or the field name of a sub-structure.
     */
    virtual void getField(GetFieldRequester::shared_pointer const & requester,std::string const & subField);

    /**
     * Not useful...
     *
     * @param pvField The field for which access rights is desired.
     * @return The access rights.
     */
    virtual AccessRights getAccessRights(epics::pvData::PVField::shared_pointer const & pvField);

    /**
     * Initiate a request for a Process action.
     *
     * ChannelProcessRequester::channelProcessConnect() may be called before createChannelProcess() returns, or at some time afterwards.
     *
     * Failure is indicated by a call to channelProcessConnect with !Error::isOk()
     *
     * @pre The Channel need not be CONNECTED
     *
     * @post The returned ChannelProcess will hold a strong reference to the provided ChannelProcessRequester.
     *
     * @post Returned shared_ptr<ChannelProcess> will have unique()==true.
     *
     * @return A non-NULL ChannelProcess unless channelProcessConnect() called with an Error
     *
     * @note The default implementation proxies using createChannelPut() and ChannelPut::put() with no data (empty bit set)
     */
    virtual ChannelProcess::shared_pointer createChannelProcess(
        ChannelProcessRequester::shared_pointer const & requester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    /**
     * Initiate a request for a Get action.
     *
     * ChannelGetRequester::channelGetConnect() may be called before createChannelGet() returns, or at some time afterwards.
     *
     * Failure is indicated by a call to channelProcessConnect with !Error::isOk()
     *
     * @pre The Channel need not be CONNECTED
     *
     * @post The returned ChannelGet will hold a strong reference to the provided ChannelGetRequester.
     *
     * @post Returned shared_ptr<ChannelGet> will have unique()==true.
     *
     * @return A non-NULL ChannelGet unless channelGetConnect() called with an Error
     *
     * @note The default implementation proxies to createChannelPut()
     */
    virtual ChannelGet::shared_pointer createChannelGet(
        ChannelGetRequester::shared_pointer const & requester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    /**
     * Initiate a request for a Put action.
     *
     * ChannelPutRequester::channelPutConnect() may be called before createChannelPut() returns, or at some time afterwards.
     *
     * Failure is indicated by a call to channelProcessConnect with !Error::isOk()
     *
     * @pre The Channel need not be CONNECTED
     *
     * @post The returned ChannelPut will hold a strong reference to the provided ChannelPutRequester.
     *
     * @post Returned shared_ptr<ChannelPut> will have unique()==true.
     *
     * @return A non-NULL ChannelPut unless channelPutConnect() called with an Error
     *
     * @note The default implementation yields a not implemented error
     */
    virtual ChannelPut::shared_pointer createChannelPut(
        ChannelPutRequester::shared_pointer const & requester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    /**
     * Initiate a request for a PutGet action.
     *
     * ChannelPutGetRequester::channelPutGetConnect() may be called before createChannelPutGet() returns, or at some time afterwards.
     *
     * Failure is indicated by a call to channelProcessConnect with !Error::isOk()
     *
     * @pre The Channel need not be CONNECTED
     *
     * @post The returned ChannelPutGet will hold a strong reference to the provided ChannelPutGetRequester.
     *
     * @post Returned shared_ptr<ChannelPutGet> will have unique()==true.
     *
     * @return A non-NULL ChannelPutGet unless channelPutGetConnect() called with an Error
     *
     * @note The default implementation yields a not implemented error
     */
    virtual ChannelPutGet::shared_pointer createChannelPutGet(
        ChannelPutGetRequester::shared_pointer const & requester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    /**
     * Initiate a request for a RPC action.
     *
     * ChannelRPCRequester::channelRPCConnect() may be called before createChannelRPC() returns, or at some time afterwards.
     *
     * Failure is indicated by a call to channelProcessConnect with !Error::isOk()
     *
     * @pre The Channel need not be CONNECTED
     *
     * @post The returned ChannelRPC will hold a strong reference to the provided ChannelRPCRequester.
     *
     * @post Returned shared_ptr<ChannelRPC> will have unique()==true.
     *
     * @return A non-NULL ChannelRPC unless channelRPCConnect() called with an Error
     *
     * @note The default implementation yields a not implemented error
     */
    virtual ChannelRPC::shared_pointer createChannelRPC(
        ChannelRPCRequester::shared_pointer const & requester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    /**
     * Initiate a request for a Monitor action.
     *
     * MonitorRequester::channelMonitorConnect() may be called before createMonitor() returns, or at some time afterwards.
     *
     * Failure is indicated by a call to monitorConnect with !Error::isOk()
     *
     * @pre The Channel need not be CONNECTED
     *
     * @post The returned Monitor will hold a strong reference to the provided MonitorRequester.
     *
     * @post Returned shared_ptr<Monitor> will have unique()==true.
     *
     * @return A non-NULL Monitor unless monitorConnect() called with an Error
     *
     * @note The default implementation yields a not implemented error
     */
    virtual Monitor::shared_pointer createMonitor(
        MonitorRequester::shared_pointer const & requester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    /**
     * Initiate a request for a Array (get) action.
     *
     * ChannelArrayRequester::channelArrayConnect() may be called before createChannelArray() returns, or at some time afterwards.
     *
     * Failure is indicated by a call to channelArrayConnect with !Error::isOk()
     *
     * @pre The Channel need not be CONNECTED
     *
     * @post The returned ChannelArray will hold a strong reference to the provided MonitorRequester.
     *
     * @post Returned shared_ptr<ChannelArray> will have unique()==true.
     *
     * @return A non-NULL ChannelArray unless channelArrayConnect() called with an Error
     *
     * Create a ChannelArray.
     * @param channelArrayRequester The ChannelArrayRequester
     * @param pvRequest Additional options (e.g. triggering).
     * @return <code>ChannelArray</code> instance.
     *
     * @note The default implementation yields a not implemented error
     */
    virtual ChannelArray::shared_pointer createChannelArray(
        ChannelArrayRequester::shared_pointer const & requester,
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
 * Event notifications associated with Channel life-cycle.
 *
 * See ChannelProvider::createChannel()
 */
class epicsShareClass ChannelRequester : public virtual Requester
{
    ChannelRequester(const ChannelRequester&);
    ChannelRequester& operator=(const ChannelRequester&);
public:
    POINTER_DEFINITIONS(ChannelRequester);
    typedef Channel operation_type;

    static size_t num_instances;

    ChannelRequester();
    virtual ~ChannelRequester();

    /**
     * The request made with ChannelProvider::createChannel() is satisfied.
     *
     * Will be called at most once for each call to createChannel().
     *
     * The Channel passed here must be the same as was returned by createChannel(), if it has returned.
     * Note that this method may be called before createChanel() returns.
     *
     * Status::isOk() indicates that the Channel is valid.
     * Calls to Channel methods can be made from this method, and later until Channel::destroy() is called.
     *
     * !Status::isOk() indicates that the Channel is not available.
     * No calls to the Channel are permitted.
     * channelStateChange() will never be called.
     *
     * Caller must hold no locks.
     *
     * @param status Completion status.
     * @param channel The channel.
     */
    virtual void channelCreated(const epics::pvData::Status& status, Channel::shared_pointer const & channel) = 0;

    /**
     * Called occasionally after channelCreated() with Status::isOk() to give notification of
     * connection state changes.
     *
     * Caller must hold no locks.
     *
     * @param c The channel.
     * @param connectionState The new connection state.
     */
    virtual void channelStateChange(Channel::shared_pointer const & channel, Channel::ConnectionState connectionState) = 0;

    /**
     * @brief Return information on connected peer if applicable.
     *
     * A server-type ChannelProvider will use this method to discover if a remote client
     * has provided credentials which may be used in access control decisions.
     *
     * Default implementation returns NULL.
     *
     * isConnected()==true and getPeerInfo()==NULL when the ChannelProvider does not provide
     * information about the peer.  This should be treated as an unauthenticated, anonymous,
     * peer.
     *
     * The returned instance must not change, and a different instance should be returned
     * if/when peer information changes (eg. after reconnect).
     *
     * May return !NULL when !isConnected().  getPeerInfo() must be called _before_
     * testing isConnected() in situations where connection state is being polled.
     */
    virtual std::tr1::shared_ptr<const PeerInfo> getPeerInfo();
};

//! Used when ChannelProvider::createChannel() is passed a NULL ChannelRequester
struct epicsShareClass DefaultChannelRequester : public ChannelRequester
{
    virtual ~DefaultChannelRequester() {}
    virtual std::string getRequesterName() OVERRIDE FINAL;
    virtual void channelCreated(const epics::pvData::Status& status, Channel::shared_pointer const & channel) OVERRIDE FINAL;
    virtual void channelStateChange(Channel::shared_pointer const & channel, Channel::ConnectionState connectionState) OVERRIDE FINAL;
    static ChannelRequester::shared_pointer build();
};

/**
 * @brief The FlushStrategy enum
 */
enum FlushStrategy {
    IMMEDIATE, DELAYED, USER_CONTROLED
};

/**
 * An instance of a Client or Server.
 *
 * Uniquely configurable (via ChannelProviderFactory::newInstance(Configuration*)
 */
class epicsShareClass ChannelProvider : public Destroyable
{
    EPICS_NOT_COPYABLE(ChannelProvider)
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

    static size_t num_instances;

    ChannelProvider();
    virtual ~ChannelProvider();

    /**
     * Get the provider name.
     * @return The name.
     */
    virtual std::string getProviderName() = 0;

    /**
     * Test to see if this provider has the named channel.
     *
     * May call ChannelFindRequester::channelFindResult() before returning, or at some time later.
     * If an exception is thrown, then channelFindResult() will never be called.
     *
     * @param name The channel name.
     * @param requester The Requester.
     * @return An unique()==true handle for the pending response.  May only return NULL if channelFindResult() called with an Error
     */
    virtual ChannelFind::shared_pointer channelFind(std::string const & name,
            ChannelFindRequester::shared_pointer const & requester) = 0;

    /**
     * Request a list of all valid channel names for this provider.
     *
     * May call ChannelListRequester::channelListResult() before returning, or at some time later.
     * If an exception is thrown, then channelListResult() will never be called.
     *
     * @param requester The Requester.
     * @return An unique()==true handle for the pending response.  May only return NULL if channelFindResult() called with an Error
     */
    virtual ChannelFind::shared_pointer channelList(ChannelListRequester::shared_pointer const & requester);

    /**
     * See longer form
     */
    virtual Channel::shared_pointer createChannel(std::string const & name,
                                                  ChannelRequester::shared_pointer const & requester = DefaultChannelRequester::build(),
                                                  short priority = PRIORITY_DEFAULT);

    /**
     * Request a Channel.
     *
     * Channel creation is immediate.
     * ChannelRequester::channelCreated() will be called before returning.
     * The shared_ptr which is passed to channelCreated() will also be returned.
     *
     * Failures during channel creation are delivered to ChannelRequester::channelCreated() with Status::isSuccess()==false.
     *
     * @post The returned Channel will hold a strong reference to the provided ChannelRequester
     * and to the ChannelProvider through which it is created.
     *
     * @post The shared_ptr passed to ChannelRequester::channelCreated() is unique.  See @ref providers_ownership_unique
     *
     * @post The new Channel will _not_ hold a strong reference to this ChannelProvider.
     *       This provider must be kept alive in order to keep the Channel from being destoryed.
     *
     * @param name The name of the channel.
     * @param requester Will receive notifications about channel state changes
     * @param priority channel priority, must be <code>PRIORITY_MIN</code> <= priority <= <code>PRIORITY_MAX</code>.
     * @param address Implementation dependent condition.  eg. A network address to bypass the search phase.  Pass an empty() string for default behavour.
     * @return A non-NULL Channel unless channelCreated() called with an Error
     */
    virtual Channel::shared_pointer createChannel(std::string const & name,ChannelRequester::shared_pointer const & requester,
            short priority, std::string const & address) = 0;
};

/**
 * <code>ChanneProvider</code> factory interface.
 */
class epicsShareClass ChannelProviderFactory {
    EPICS_NOT_COPYABLE(ChannelProviderFactory)
public:
    POINTER_DEFINITIONS(ChannelProviderFactory);

    ChannelProviderFactory() {}
    virtual ~ChannelProviderFactory() {}

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

//! Simple ChannelProviderFactory which requires the existance of a ctor
//!   Provider(const std::tr1::shared_ptr<Configuration>& conf)
//! which is called with a Configuration instance or NULL (use some defaults)
template<class Provider>
struct SimpleChannelProviderFactory : public ChannelProviderFactory
{
    SimpleChannelProviderFactory(const std::string& name) :pname(name) {}
    virtual ~SimpleChannelProviderFactory() {}

    virtual std::string getFactoryName() OVERRIDE FINAL { return pname; }

    virtual ChannelProvider::shared_pointer sharedInstance() OVERRIDE FINAL
    {
        epics::pvData::Lock L(sharedM);
        ChannelProvider::shared_pointer ret(shared.lock());
        if(!ret) {
            std::tr1::shared_ptr<Provider> inst(new Provider(std::tr1::shared_ptr<Configuration>()));
            shared = ret = inst;
        }
        return ret;
    }

    virtual ChannelProvider::shared_pointer newInstance(const std::tr1::shared_ptr<Configuration>& conf) OVERRIDE FINAL
    {
        std::tr1::shared_ptr<Provider> ret(new Provider(conf));
        return ret;
    }

private:
    const std::string pname;
    epics::pvData::Mutex sharedM;
    ChannelProvider::weak_pointer shared;
};

//! Helper for ChannelProviders which access a singleton resource (eg. process database).
//! Only one concurrent instance will be created.
//! Requires the existance of a ctor
//!   Provider(const std::tr1::shared_ptr<const Configuration>& conf)
template<class Provider>
struct SingletonChannelProviderFactory : public ChannelProviderFactory
{
    SingletonChannelProviderFactory(const std::string& name,
                                    const std::tr1::shared_ptr<const Configuration>& conf = std::tr1::shared_ptr<const Configuration>())
        :pname(name), config(conf)
    {}
    virtual ~SingletonChannelProviderFactory() {}

    virtual std::string getFactoryName() OVERRIDE FINAL { return pname; }

    virtual ChannelProvider::shared_pointer sharedInstance() OVERRIDE FINAL
    {
        epics::pvData::Lock L(sharedM);
        ChannelProvider::shared_pointer ret(shared.lock());
        if(!ret) {
            std::tr1::shared_ptr<Provider> inst(new Provider(config));
            shared = ret = inst;
        }
        return ret;
    }

    virtual ChannelProvider::shared_pointer newInstance(const std::tr1::shared_ptr<Configuration>& conf) OVERRIDE FINAL
    {
        (void)conf; // ignore and use our Configuration
        return sharedInstance();
    }
private:
    const std::string pname;
    epics::pvData::Mutex sharedM;
    ChannelProvider::weak_pointer shared;
    const std::tr1::shared_ptr<const Configuration> config;
};

/**
 * Interface for locating channel providers.
 */
class epicsShareClass ChannelProviderRegistry {
public:
    POINTER_DEFINITIONS(ChannelProviderRegistry);

    typedef std::vector<std::string> stringVector_t;

    virtual ~ChannelProviderRegistry() {}


    //! Create a custom registry
    static ChannelProviderRegistry::shared_pointer build();
    //! The global registry for "clients".
    //! Register providers which will be used within this process.
    //! Typically providers which access data outside of the process.
    //! Associated with EPICS_PVA_PROVIDER_NAMES
    static ChannelProviderRegistry::shared_pointer clients();
    //! The global registry for "servers".
    //! Register providers which will be used outside of this process (via ServerContext).
    //! Typically providers which access data within the process.
    //! Associated with EPICS_PVAS_PROVIDER_NAMES
    static ChannelProviderRegistry::shared_pointer servers();

    /**
     * Get a shared instance of the provider with the specified name.
     * @param providerName The name of the provider.
     * @return The interface for the provider or null if the provider is not known.
     */
    ChannelProvider::shared_pointer getProvider(std::string const & providerName);

    /**
     * Creates a new instanceof the provider with the specified name.
     * @param providerName The name of the provider.
     * @return The interface for the provider or null if the provider is not known.
     */
    ChannelProvider::shared_pointer createProvider(std::string const & providerName,
                                                           const std::tr1::shared_ptr<Configuration>& conf = std::tr1::shared_ptr<Configuration>());

    /**
     * Fetch provider factor with the specified name.
     * @param providerName The name of the provider.
     * @return The factor or null if the provider is not known.
     */
    virtual ChannelProviderFactory::shared_pointer getFactory(std::string const & providerName);

    typedef std::set<std::string> provider_name_set;
    /**
     * Find currently registered provider names.
     */
    virtual void getProviderNames(provider_name_set& names);

    //! Add new factory.  if replace=false and name already in use, return false with no change
    //! in other cases insert provided factory and return true.
    virtual bool add(const ChannelProviderFactory::shared_pointer& fact, bool replace=true);

    //! Add a new Provider which will be built using SimpleChannelProviderFactory<Provider>
    template<class Provider>
    ChannelProviderFactory::shared_pointer add(const std::string& name, bool replace=true)
    {
        typedef SimpleChannelProviderFactory<Provider> Factory;
        typename Factory::shared_pointer fact(new Factory(name));
        return add(fact, replace) ? fact : typename Factory::shared_pointer();
    }

    typedef ChannelProvider::shared_pointer (*factoryfn_t)(const std::tr1::shared_ptr<Configuration>&);

    ChannelProviderFactory::shared_pointer add(const std::string& name, factoryfn_t, bool replace=true);

    //! Add a new Provider which will be built using SingletonChannelProviderFactory<Provider>
    template<class Provider>
    ChannelProviderFactory::shared_pointer addSingleton(const std::string& name,
                                                        const std::tr1::shared_ptr<const Configuration>& conf = std::tr1::shared_ptr<const Configuration>(),
                                                        bool replace=true)
    {
        typedef SingletonChannelProviderFactory<Provider> Factory;
        typename Factory::shared_pointer fact(new Factory(name, conf));
        return add(fact, replace) ? fact : typename Factory::shared_pointer();
    }

    //! Add a pre-created Provider instance.
    //! Only a weak ref to this instance is kept, so the instance must be kept active
    //! through some external means
    //! @since 6.1.0
    ChannelProviderFactory::shared_pointer addSingleton(const ChannelProvider::shared_pointer& provider,
                                                        bool replace=true);

    //! Attempt to remove a factory with the given name.  Return Factory which was removed, or NULL if not found.
    ChannelProviderFactory::shared_pointer remove(const std::string& name);

    //! Attempt to remove a factory.  Return true if Factory was previously registered, and now removed.
    virtual bool remove(const ChannelProviderFactory::shared_pointer& factory);

    //! Drop all factories
    virtual void clear();

private:
    // ctor is hidden to ensure explict compile errors from a hypothetical sub-class
    // we no longer support sub-classing by outside code
    ChannelProviderRegistry() {}
    friend struct CompatRegistry;

    epics::pvData::Mutex mutex;
    typedef std::map<std::string, ChannelProviderFactory::shared_pointer> providers_t;
    providers_t providers;
};

/* Deprecated in favor of either ChannelProviderRegistry::clients() or ChannelProviderRegistry::servers()
 *
 * These functions have been removed as a signal that the shared_ptr ownership symantics of ChannelProvider
 * and friends has changed.
 */
#if __GNUC__>4 || (__GNUC__==4&&__GNUC_MINOR__>=3)

#define gCPRMSG __attribute__((error("ChannelProvider shared_ptr ownership rules have changed.")))

epicsShareFunc ChannelProviderRegistry::shared_pointer getChannelProviderRegistry() gCPRMSG;
// Shorthand for getChannelProviderRegistry()->add(channelProviderFactory);
epicsShareFunc void registerChannelProviderFactory(ChannelProviderFactory::shared_pointer const & channelProviderFactory) gCPRMSG;
// Shorthand for getChannelProviderRegistry()->remove(channelProviderFactory);
epicsShareFunc void unregisterChannelProviderFactory(ChannelProviderFactory::shared_pointer const & channelProviderFactory) gCPRMSG;
// Shorthand for getChannelProviderRegistry()->clear();
epicsShareFunc void unregisterAllChannelProviderFactory() gCPRMSG;

#undef gCPRMSG

#endif // __GNUC__

/**
 * @brief Pipeline (streaming) support API (optional).
 * This is used by pvAccess to implement pipeline (streaming) monitors.
 */
typedef Monitor PipelineMonitor;


}
}

// compatibility
namespace epics { namespace pvData {
using epics::pvAccess::MonitorRequester;
}}


#endif  /* PVACCESS_H */

/** @page Overview Documentation
 *
 *<a href = "../documentation/pvAccessCPP.html">pvAccessCPP.html</a>
 *
 */

