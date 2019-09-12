/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
#ifndef PVATESTCLIENT_H
#define PVATESTCLIENT_H

#include <ostream>
#include <stdexcept>
#include <list>

#include <epicsMutex.h>

#include <pv/pvData.h>
#include <pv/bitSet.h>

class epicsEvent;

namespace epics {namespace pvAccess {
class ChannelProvider;
class Channel;
class Monitor;
class Configuration;
}}//namespace epics::pvAccess

//! See @ref pvac API
namespace pvac {

/** @defgroup pvac Client API
 *
 * PVAccess network client (or other epics::pvAccess::ChannelProvider)
 *
 * Usage:
 *
 * 1. Construct a ClientProvider
 * 2. Use the ClientProvider to obtain a ClientChannel
 * 3. Use the ClientChannel to begin an get, put, rpc, or monitor operation
 *
 * Code examples
 *
 * - @ref examples_getme
 * - @ref examples_putme
 * - @ref examples_monitorme
 * @{
 */

class ClientProvider;

//! Handle for in-progress get/put/rpc operation
struct epicsShareClass Operation
{
    struct Impl
    {
        virtual ~Impl() {}
        virtual std::string name() const =0;
        virtual void cancel() =0;
        virtual void show(std::ostream&) const =0;
    };

    Operation() {}
    Operation(const std::tr1::shared_ptr<Impl>&);
    ~Operation();
    //! Channel name
    std::string name() const;
    //! Immediate cancellation.
    //! Does not wait for remote confirmation.
    void cancel();

    bool valid() const { return !!impl; }

#if __cplusplus>=201103L
    explicit operator bool() const { return valid(); }
#else
private:
    typedef bool (Operation::*bool_type)() const;
public:
    operator bool_type() const { return valid() ? &Operation::valid : 0; }
#endif

    void reset() { impl.reset(); }

protected:
    friend epicsShareFunc ::std::ostream& operator<<(::std::ostream& strm, const Operation& op);
    std::tr1::shared_ptr<Impl> impl;
};

//! Information on put completion
struct epicsShareClass PutEvent
{
    enum event_t {
        Fail,    //!< request ends in failure.  Check message
        Cancel,  //!< request cancelled before completion
        Success, //!< It worked!
    } event;
    std::string message; //!< Check when event==Fail
};

//! Information on get/rpc completion
struct epicsShareClass GetEvent : public PutEvent
{
    //! New data. NULL unless event==Success
    epics::pvData::PVStructure::const_shared_pointer value;
    //! Mask of fields in value which have been initialized by the server
    //! @since 6.1.0
    epics::pvData::BitSet::const_shared_pointer valid;
};

struct epicsShareClass InfoEvent : public PutEvent
{
    //! Type description resulting from getField operation.  NULL unless event==Success
    epics::pvData::FieldConstPtr type;
};

struct MonitorSync;

//! Handle for monitor subscription
struct epicsShareClass Monitor
{
    struct Impl;
    Monitor() {}
    Monitor(const std::tr1::shared_ptr<Impl>&);
    ~Monitor();

    //! Channel name
    std::string name() const;
    //! Immediate cancellation.
    /** Does not wait for remote confirmation.
     *
     @since Up to 7.0.0 also cleared root, changed, and overrun.
            After 7.0.0, these data members are left unchanged.
     **/
    void cancel();
    /** updates root, changed, overrun
     *
     * @return true if a new update was extracted from the queue.
     * @note This method does not block.
     * @note MonitorEvent::Data will not be repeated until poll()==false.
     * @post root!=NULL (after version 6.0.0)
     * @post root!=NULL iff poll()==true  (In version 6.0.0)
     */
    bool poll();
    //! true if all events received.
    //! Check after poll()==false
    bool complete() const;
    /** Monitor update data.
     *
     * After version 6.0.0
     *
     * Initially NULL, becomes !NULL the first time poll()==true.
     * The PVStructure pointed to be root will presist until
     * Monitor reconnect w/ type change.  This can be detected
     * by comparing `root.get()`.  references to root may be cached
     * subject to this test.
     *
     * In version 6.0.0
     *
     * NULL except after poll()==true.  poll()==false sets root=NULL.
     * references to root should not be stored between calls to poll().
     */
    epics::pvData::PVStructure::const_shared_pointer root;
    epics::pvData::BitSet changed,
                          overrun;

    bool valid() const { return !!impl; }

#if __cplusplus>=201103L
    explicit operator bool() const { return valid(); }
#else
private:
    typedef bool (Monitor::*bool_type)() const;
public:
    operator bool_type() const { return valid() ? &Monitor::valid : 0; }
#endif

    void reset() { impl.reset(); }

private:
    std::tr1::shared_ptr<Impl> impl;
    friend epicsShareFunc ::std::ostream& operator<<(::std::ostream& strm, const Monitor& op);
    friend struct MonitorSync;
};

//! Information on monitor subscription/queue change
struct MonitorEvent
{
    enum event_t {
        Fail=1,      //!< subscription ends in an error
        Cancel=2,    //!< subscription ends in cancellation
        Disconnect=4,//!< subscription interrupted due to loss of communication
        Data=8,      //!< Data queue not empty.  Call Monitor::poll()
    } event;
    std::string message; //!< set for event=Fail
};

/** Subscription usable w/o callbacks
 *
 * Basic usage is to call wait() or test().
 * If true is returned, then the 'event', 'root', 'changed', and 'overrun'
 * members have been updated with a new event.
 * Test 'event.event' first to find out which kind of event has occured.
 *
 * Note that wait()/test() methods are distinct from base class poll().
 * wait()/test() check for the arrival of MonitorEvent
 * while poll() checks for the availability of data (eg. following a Data event).
 */
struct epicsShareClass MonitorSync : public Monitor
{
    struct SImpl;
    MonitorSync() {}
    MonitorSync(const Monitor&, const std::tr1::shared_ptr<SImpl>&);
    ~MonitorSync();

    //! wait for new event
    //! @returns true when a new event was received.
    //!          false if wake() was called.
    bool wait();
    //! wait for new event
    //! @return false on timeout
    bool wait(double timeout);
    //! check if new event is immediately available.
    //! Does not block.
    bool test();

    //! Abort one call to wait(), either concurrent or future.
    //! Calls are queued.
    //! wait() will return with MonitorEvent::Fail.
    void wake();

    //! most recent event
    //! updated only during wait() or poll()
    MonitorEvent event;
private:
    std::tr1::shared_ptr<SImpl> simpl;
};

//! information on connect/disconnect
struct ConnectEvent
{
    //! Is this a connection, or disconnection, event.
    bool connected;
    //! For connection events.  This is the name provided by the peer (cf. epics::pvAccess::Channel::getRemoteAddress() ).
    //! @since >6.1.0
    std::string peerName;
};

//! Thrown by blocking methods of ClientChannel on operation timeout
struct Timeout : public std::runtime_error
{
    Timeout();
};

namespace detail {
class PutBuilder;
void registerRefTrack();
}

/** Represents a single channel
 *
 * This class has two sets of methods, those which block for completion, and
 * those which use callbacks to signal completion.
 *
 * Those which block accept a 'timeout' argument (in seconds).
 *
 * Those which use callbacks accept a 'cb' argument and return an Operation or Monitor handle object.
 */
class epicsShareClass ClientChannel
{
#if defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__ < 305)
public:
    // Impl is public only as a workaround on older GCC
#endif
    struct Impl;
private:
    std::tr1::shared_ptr<Impl> impl;
    friend class ClientProvider;
    friend void detail::registerRefTrack();
    friend epicsShareFunc ::std::ostream& operator<<(::std::ostream& strm, const ClientChannel& op);

    ClientChannel(const std::tr1::shared_ptr<Impl>& i) :impl(i) {}
public:
    //! Channel creation options
    struct epicsShareClass Options {
        short priority;
        std::string address;
        Options();
        bool operator<(const Options&) const;
    };

    //! Construct a null channel.  All methods throw.  May later be assigned from a valid ClientChannel
    ClientChannel() {}
    /** Construct a ClientChannel using epics::pvAccess::ChannelProvider::createChannel()
     *
     * Does not block.
     * @throw std::logic_error if the provider is NULL or name is an empty string
     * @throw std::runtime_error if the ChannelProvider can't provide
     */
    ClientChannel(const std::tr1::shared_ptr<epics::pvAccess::ChannelProvider>& provider,
                      const std::string& name,
                      const Options& opt = Options());
    ~ClientChannel();

    //! Channel name or an empty string
    std::string name() const;

    bool valid() const { return !!impl; }

#if __cplusplus>=201103L
    explicit operator bool() const { return valid(); }
#else
private:
    typedef bool (ClientChannel::*bool_type)() const;
public:
    operator bool_type() const { return valid() ? &ClientChannel::valid : 0; }
#endif

    void reset() { impl.reset(); }

    //! callback for get() and rpc()
    struct GetCallback {
        virtual ~GetCallback() {}
        //! get or rpc operation is complete
        virtual void getDone(const GetEvent& evt)=0;
    };

    //! Issue request to retrieve current PV value
    //! @param cb Completion notification callback.  Must outlive Operation (call Operation::cancel() to force release)
    //! @param pvRequest if NULL defaults to "field()".
    Operation get(GetCallback* cb,
                      epics::pvData::PVStructure::const_shared_pointer pvRequest = epics::pvData::PVStructure::const_shared_pointer());

    //! Block and retrieve current PV value
    //! @param timeout in seconds
    //! @param pvRequest if NULL defaults to "field()".
    //! @throws Timeout or std::runtime_error
    epics::pvData::PVStructure::const_shared_pointer
    get(double timeout = 3.0,
        epics::pvData::PVStructure::const_shared_pointer pvRequest = epics::pvData::PVStructure::const_shared_pointer());


    //! Start an RPC call
    //! @param cb Completion notification callback.  Must outlive Operation (call Operation::cancel() to force release)
    //! @param arguments encoded call arguments
    //! @param pvRequest if NULL defaults to "field()".
    Operation rpc(GetCallback* cb,
                      const epics::pvData::PVStructure::const_shared_pointer& arguments,
                      epics::pvData::PVStructure::const_shared_pointer pvRequest = epics::pvData::PVStructure::const_shared_pointer());

    //! Block and execute remote call
    //! @param timeout in seconds
    //! @param arguments encoded call arguments
    //! @param pvRequest if NULL defaults to "field()".
    epics::pvData::PVStructure::const_shared_pointer
    rpc(double timeout,
        const epics::pvData::PVStructure::const_shared_pointer& arguments,
        epics::pvData::PVStructure::const_shared_pointer pvRequest = epics::pvData::PVStructure::const_shared_pointer());

    //! callbacks for put()
    struct PutCallback {
        virtual ~PutCallback() {}
        struct Args {
            Args(epics::pvData::BitSet& tosend, epics::pvData::BitSet& previousmask) :tosend(tosend), previousmask(previousmask) {}
            //! Callee must fill this in with an instance of the Structure passed as the 'build' argument.
            epics::pvData::PVStructure::const_shared_pointer root;
            //! Callee must set bits corresponding to the fields of 'root' which will actually be sent.
            epics::pvData::BitSet& tosend;
            //! A previous value of the PV being "put" when put(..., getprevious=true).  eg. use to find enumeration value.
            //! Otherwise NULL.
            //! @note The value of the PV may change between the point where "previous" is fetched,
            //!       and when this Put operation completes.
            //! @since 6.1.0 Added after 6.0.0
            epics::pvData::PVStructure::const_shared_pointer previous;
            //! Bit mask indicating those fields of 'previous' which have been set by the server.  (others have local defaults)
            //! Unused if previous==NULL.
            const epics::pvData::BitSet& previousmask;
        };
        /** Server provides expected structure.
         *
         * Implementation must instanciate (or re-use) a PVStructure into args.root,
         * then initialize any necessary fields and set bits in args.tosend as approprate.
         *
         * If this method throws, then putDone() is called with PutEvent::Fail
         */
        virtual void putBuild(const epics::pvData::StructureConstPtr& build, Args& args) =0;
        //! Put operation is complete
        virtual void putDone(const PutEvent& evt)=0;
    };

    //! Initiate request to change PV
    //! @param cb Completion notification callback.  Must outlive Operation (call Operation::cancel() to force release)
    //! @param pvRequest if NULL defaults to "field()".
    //! @param getprevious If true, fetch a previous value of the PV and make
    //!                    this available as PutCallback::Args::previous and previousmask.
    //!                    If false, then previous=NULL
    Operation put(PutCallback* cb,
                  epics::pvData::PVStructure::const_shared_pointer pvRequest = epics::pvData::PVStructure::const_shared_pointer(),
                  bool getprevious = false);

    //! Synchronious put operation
    inline
    detail::PutBuilder put(const epics::pvData::PVStructure::const_shared_pointer &pvRequest = epics::pvData::PVStructure::const_shared_pointer());

    //! Monitor event notification
    struct MonitorCallback {
        virtual ~MonitorCallback() {}
        /** New monitor event
         *
         * - MonitorEvent::Fail - An Error occurred.  Check evt.message
         * - MonitorEvent::Cancel - Monitor::cancel() called
         * - MonitorEvent::Disconnect - Underlying ClientChannel becomes disconnected
         * - MonitorEvent::Data - FIFO becomes not empty.Call Monitor::poll()
         */
        virtual void monitorEvent(const MonitorEvent& evt)=0;
    };

    //! Begin subscription
    //! @param cb Completion notification callback.  Must outlive Monitor (call Monitor::cancel() to force release)
    Monitor monitor(MonitorCallback *cb,
                          epics::pvData::PVStructure::const_shared_pointer pvRequest = epics::pvData::PVStructure::const_shared_pointer());

    /** Begin subscription w/o callbacks
     *
     * @param event If not NULL, then subscription events are signaled to this epicsEvent.
     *        Use MonitorSync::test() to see if a subscription has an event waiting.
     *        Otherwise an internal epicsEvent is allocated for use with MonitorSync::wait()
     *
     * @note For simple usage with a single MonitorSync, pass event=NULL and call MonitorSync::wait().
     *       If more than one MonitorSync is being created, then pass a custom epicsEvent and use MonitorSync::test() to find
     *       which subscriptions have events pending.
     */
    MonitorSync monitor(const epics::pvData::PVStructure::const_shared_pointer& pvRequest = epics::pvData::PVStructure::const_shared_pointer(),
                        epicsEvent *event =0);

    struct InfoCallback {
        virtual ~InfoCallback() {}
        //! getField operation is complete
        virtual void infoDone(const InfoEvent& evt) =0;
    };

    //! Request PV type info.
    //! @note This type may not be the same as the types used in the get/put/monitor operations.
    Operation info(InfoCallback *cb, const std::string& subfld = std::string());

    //! Synchronious getField opreation
    epics::pvData::FieldConstPtr info(double timeout = 3.0,
                                      const std::string& subfld = std::string());

    //! Connection state change CB
    struct ConnectCallback {
        virtual ~ConnectCallback() {}
        virtual void connectEvent(const ConnectEvent& evt)=0;
    };
    //! Append to list of listeners
    //! @param cb Channel dis/connect notification callback.  Must outlive ClientChannel or call to removeConnectListener()
    void addConnectListener(ConnectCallback*);
    //! Remove from list of listeners
    void removeConnectListener(ConnectCallback*);

    void show(std::ostream& strm) const;
private:
    std::tr1::shared_ptr<epics::pvAccess::Channel> getChannel();
};

namespace detail {

//! Helper to accumulate values to for a Put operation.
//! Make sure to call exec() to begin operation.
class epicsShareClass PutBuilder {
    ClientChannel& channel;
    epics::pvData::PVStructure::const_shared_pointer request;

    template<typename V>
    struct triple {
        std::string name;
        bool required;
        V value;
        triple(const std::string& name, const V& value, bool required =true)
            :name(name), required(required), value(value)
        {}
    };

    typedef std::list<triple<epics::pvData::AnyScalar> > scalars_t;
    scalars_t scalars;

    typedef std::list<triple<epics::pvData::shared_vector<const void> > > arrays_t;
    arrays_t arrays;

    struct Exec;

    friend class pvac::ClientChannel;
    PutBuilder(ClientChannel& channel, const epics::pvData::PVStructure::const_shared_pointer& request)
        :channel(channel), request(request)
    {}
public:
    PutBuilder& set(const std::string& name, const epics::pvData::AnyScalar& value, bool required=true) {
        scalars.push_back(scalars_t::value_type(name, value, required));
        return *this;
    }
    template<typename T>
    PutBuilder& set(const std::string& name, T value, bool required=true) {
        return set(name, epics::pvData::AnyScalar(value), required);
    }
    PutBuilder& set(const std::string& name, const epics::pvData::shared_vector<const void>& value, bool required=true) {
        arrays.push_back(arrays_t::value_type(name, value, required));
        return *this;
    }
    template<typename T>
    PutBuilder& set(const std::string& name, const epics::pvData::shared_vector<const T>& value, bool required=true) {
        return set(name, epics::pvData::static_shared_vector_cast<const void>(value), required);
    }
    void exec(double timeout=3.0);
};


}// namespace detail

//! Central client context.
class epicsShareClass ClientProvider
{
    struct Impl;
    std::tr1::shared_ptr<Impl> impl;
    friend void detail::registerRefTrack();
    friend epicsShareFunc ::std::ostream& operator<<(::std::ostream& strm, const ClientProvider& op);
public:

    //! Construct a null provider.  All methods throw.  May later be assigned from a valid ClientProvider
    ClientProvider() {}
    /** Use named provider.
     *
     * @param providerName ChannelProvider name, may be prefixed with "clients:" or "servers:" to query
     *        epics::pvAccess::ChannelProviderRegistry::clients() or
     *        epics::pvAccess::ChannelProviderRegistry::servers().
     *        No prefix implies "clients:".
     */
    ClientProvider(const std::string& providerName,
                   const std::tr1::shared_ptr<epics::pvAccess::Configuration>& conf = std::tr1::shared_ptr<epics::pvAccess::Configuration>());
    explicit ClientProvider(const std::tr1::shared_ptr<epics::pvAccess::ChannelProvider>& provider);
    ~ClientProvider();

    std::string name() const;

    /** Get a new Channel
     *
     * Does not block.
     * Never returns NULL.
     * Uses internal Channel cache.
     */
    ClientChannel connect(const std::string& name,
                          const ClientChannel::Options& conf = ClientChannel::Options());

    //! Remove from channel cache
    bool disconnect(const std::string& name,
                    const ClientChannel::Options& conf = ClientChannel::Options());

    //! Clear channel cache
    void disconnect();

    bool valid() const { return !!impl; }

#if __cplusplus>=201103L
    explicit operator bool() const { return valid(); }
#else
private:
    typedef bool (ClientProvider::*bool_type)() const;
public:
    operator bool_type() const { return valid() ? &ClientProvider::valid : 0; }
#endif

    void reset() { impl.reset(); }
};



detail::PutBuilder
ClientChannel::put(const epics::pvData::PVStructure::const_shared_pointer& pvRequest)
{
    return detail::PutBuilder(*this, pvRequest);
}

epicsShareFunc ::std::ostream& operator<<(::std::ostream& strm, const Operation& op);
epicsShareFunc ::std::ostream& operator<<(::std::ostream& strm, const Monitor& op);
epicsShareFunc ::std::ostream& operator<<(::std::ostream& strm, const ClientChannel& op);
epicsShareFunc ::std::ostream& operator<<(::std::ostream& strm, const ClientProvider& op);

//! @}

}//namespace pvac

#endif // PVATESTCLIENT_H
