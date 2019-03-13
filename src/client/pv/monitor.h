/* monitor.h */
/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
/**
 *  @author mrk
 */
#ifndef MONITOR_H
#define MONITOR_H

#include <list>
#include <ostream>

#ifdef epicsExportSharedSymbols
#   define monitorEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <epicsMutex.h>
#include <pv/status.h>
#include <pv/pvData.h>
#include <pv/sharedPtr.h>
#include <pv/bitSet.h>
#include <pv/createRequest.h>

#ifdef monitorEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef monitorEpicsExportSharedSymbols
#endif

#include <pv/requester.h>
#include <pv/destroyable.h>

#include <shareLib.h>

namespace epics { namespace pvAccess {

class MonitorRequester;
class MonitorElement;
typedef std::tr1::shared_ptr<MonitorElement> MonitorElementPtr;
typedef std::vector<MonitorElementPtr> MonitorElementPtrArray;

class Monitor;
typedef std::tr1::shared_ptr<Monitor> MonitorPtr;


/**
 * @brief An element for a monitorQueue.
 *
 * Class instance representing monitor element.
 * @author mrk
 */
class epicsShareClass MonitorElement {
public:
    POINTER_DEFINITIONS(MonitorElement);
    MonitorElement(epics::pvData::PVStructurePtr const & pvStructurePtr);
    const epics::pvData::PVStructurePtr pvStructurePtr;
    const epics::pvData::BitSet::shared_pointer changedBitSet;
    const epics::pvData::BitSet::shared_pointer overrunBitSet;

    class Ref;
};

/** Access to Monitor subscription and queue
 *
 * Downstream interface to access a monitor queue (via poll() and release() )
 */
class epicsShareClass Monitor : public virtual Destroyable{
    public:
    POINTER_DEFINITIONS(Monitor);
    typedef MonitorRequester requester_type;

    virtual ~Monitor(){}
    /**
     * Start monitoring.
     * @return completion status.
     */
    virtual epics::pvData::Status start() = 0;
    /**
     * Stop Monitoring.
     * @return completion status.
     */
    virtual epics::pvData::Status stop() = 0;
    /**
     * If monitor has occurred return data.
     * @return monitorElement for modified data.
     * Must call get to determine if data is available.
     *
     * May recursively call MonitorRequester::unlisten()
     */
    virtual MonitorElementPtr poll() = 0;
    /**
     * Release a MonitorElement that was returned by poll.
     * A poll() must be called after the release() to check the presence of any modified data.
     * @param monitorElement
     */
    virtual void release(MonitorElementPtr const & monitorElement) = 0;

    struct Stats {
        size_t nfilled; //!< # of elements ready to be poll()d
        size_t noutstanding; //!< # of elements poll()d but not released()d
        size_t nempty; //!< # of elements available for new remote data
    };

    virtual void getStats(Stats& s) const {
        s.nfilled = s.noutstanding = s.nempty = 0;
    }

    /**
     * Report remote queue status.
     * @param freeElements number of free elements.
     */
    virtual void reportRemoteQueueStatus(epics::pvData::int32 freeElements) {}
};

/** A (single ownership) smart pointer to extract a MonitorElement from a Monitor queue
 *
 * To fetch a single element
 @code
   epics::pvAccess::Monitor::shared_pointer mon(....);
   epics::pvAccess::MonitorElement::Ref elem(mon);
   if(elem) {
      // do something with element
      assert(elem->pvStructurePtr->getSubField("foo"));
   } else {
      // queue was empty
   }
 @endcode
 * To fetch all available elements (c++11)
 @code
   epics::pvAccess::Monitor::shared_pointer mon(....);
   for(auto& elem : *mon) {
      assert(elem.pvStructurePtr->getSubField("foo"));
   }
 @endcode
 * To fetch all available elements (c++98)
 @code
   epics::pvAccess::Monitor::shared_pointer mon(....);
   for(epics::pvAccess::MonitorElement::Ref it(mon); it; ++it) {
      MonitorElement& elem(*it);
      assert(elem.pvStructurePtr->getSubField("foo"));
   }
 @endcode
 */
class MonitorElement::Ref
{
    Monitor* mon;
    MonitorElementPtr elem;
public:
    Ref() :mon(0), elem() {}
    Ref(Monitor& M) :mon(&M), elem(mon->poll()) {}
    Ref(const Monitor::shared_pointer& M) :mon(M.get()), elem(mon->poll()) {}
    ~Ref() { reset(); }
#if __cplusplus>=201103L
    Ref(Ref&& o) :mon(o.mon), elem(o.elem) {
        o.mon = 0;
        o.elem.reset();
    }
#endif
    void swap(Ref& o) {
        std::swap(mon , o.mon);
        std::swap(elem, o.elem);
    }
    //! analogous to auto_ptr<>::release() but given a different name
    //! to avoid being confused with Monitor::release()
    MonitorElementPtr letGo() {
        MonitorElementPtr ret;
        elem.swap(ret);
        return ret;
    }
    void attach(Monitor& M) {
        reset();
        mon = &M;
    }
    void attach(const Monitor::shared_pointer& M) {
        reset();
        mon = M.get();
    }
    bool next() {
        if(elem) mon->release(elem);
        elem = mon->poll();
        return !!elem;
    }
    void reset() {
        if(elem && mon) mon->release(elem);
        elem.reset();
    }
    Ref& operator++() {// prefix increment.  aka "++(*this)"
        next();
        return *this;
    }
#if __cplusplus>=201103L
    inline explicit operator bool() const { return elem.get(); }
#else
private:
    typedef const Monitor* const * hidden_bool_type;
public:
    operator hidden_bool_type() const { return elem.get() ? &mon : 0; }
#endif
    inline MonitorElement* operator->() { return elem.get(); }
    inline MonitorElement& operator*() { return *elem; }
    inline MonitorElement* get() { return elem.get(); }

    inline bool operator==(const Ref& o) const { return elem==o.elem; }
    inline bool operator!=(const Ref& o) const { return !(*this==o); }

    EPICS_NOT_COPYABLE(Ref)
};

#if __cplusplus>=201103L
// used by c++11 for-range
inline MonitorElement::Ref begin(Monitor& mon) { return MonitorElement::Ref(mon); }
inline MonitorElement::Ref end(Monitor& mon) { return MonitorElement::Ref(); }
#endif // __cplusplus<201103L

/** Utility implementation of Monitor.
 *
 * The Monitor interface defines the downstream (consumer facing) side
 * of a FIFO.  This class is a concrete implementation of this FIFO,
 * including the upstream (producer facing) side.
 *
 * In addition to MonitorRequester, which provides callbacks to the downstream side,
 * The MonitorFIFO::Source class provides callbacks to the upstream side.
 *
 * The simplest usage is to create (as shown below), then put update into the FIFO
 * using post() and tryPost().  These methods behave the same when the queue is
 * not full, but differ when it is.  Additionally, tryPost() has an argument 'force'.
 * Together there are three actions
 *
 * # post(value, changed) - combines the new update with the last (most recent) in the FIFO.
 * # tryPost(value, changed, ..., false) - Makes no change to the FIFO and returns false.
 * # tryPost(value, changed, ..., true) - Over-fills the FIFO with the new element, then returns false.
 *
 * @note Calls to post() or tryPost() __must__ be followed with a call to notify().
 *       Callers of notify() __must__ not hold any locks, or a deadlock is possible.
 *
 * The intent of tryPost() with force=true is to aid code which is transferring values from
 * some upstream buffer and this FIFO.  Such code can be complicated if an item is removed
 * from the upstream buffer, but can't be put into this downstream FIFO.  Rather than
 * being forced to effectivly maintain a third FIFO, code can use force=true.
 *
 * In either case, tryPost()==false indicates the the FIFO is full.
 *
 * eg. simple usage in a sub-class for Channel named MyChannel.
 @code
    pva::Monitor::shared_pointer
    MyChannel::createMonitor(const pva::MonitorRequester::shared_pointer &requester,
                             const pvd::PVStructure::shared_pointer &pvRequest)
    {
        std::tr1::shared_ptr<pva::MonitorFIFO> ret(new pva::MonitorFIFO(requester, pvRequest));
        ret->open(spamtype);
        ret->notify();
        // ret->post(...); // maybe initial update
    }
 @endcode
 */
class epicsShareClass MonitorFIFO : public Monitor,
                                    public std::tr1::enable_shared_from_this<MonitorFIFO>
{
public:
    POINTER_DEFINITIONS(MonitorFIFO);
    //! Source methods may be called with downstream mutex locked.
    //! Do not call notify().  This is done automatically after return in a way
    //! which avoids locking and recursion problems.
    struct epicsShareClass Source {
        POINTER_DEFINITIONS(Source);
        virtual ~Source();
        //! Called when MonitorFIFO::freeCount() rises above the level computed
        //! from MonitorFIFO::setFreeHighMark().
        //! @param numEmpty The number of empty slots in the FIFO.
        virtual void freeHighMark(MonitorFIFO *mon, size_t numEmpty) {}
    };
    struct epicsShareClass Config {
        size_t maxCount,    //!< upper limit on requested FIFO size
               defCount,    //!< FIFO size when client makes no request
               actualCount; //!< filled in with actual FIFO size
        bool dropEmptyUpdates; //!< default true.  Drop updates which don't include an field values.
        epics::pvData::PVRequestMapper::mode_t mapperMode; //!< default Mask.  @see epics::pvData::PVRequestMapper::mode_t
        Config();
    };

    /**
     * @param requester Downstream/consumer callbacks
     * @param pvRequest Downstream provided options
     * @param source Upstream/producer callbacks
     * @param conf Upstream provided options.  Updated with actual values used.  May be NULL to use defaults.
     */
    MonitorFIFO(const std::tr1::shared_ptr<MonitorRequester> &requester,
                const pvData::PVStructure::const_shared_pointer &pvRequest,
                const Source::shared_pointer& source = Source::shared_pointer(),
                Config *conf=0);
    virtual ~MonitorFIFO();

    //! Access to MonitorRequester passed to ctor, or NULL if it has already been destroyed.
    //! @since >6.1.0
    inline const std::tr1::shared_ptr<MonitorRequester> getRequester() const { return requester.lock(); }

    void show(std::ostream& strm) const;

    virtual void destroy() OVERRIDE FINAL;

    // configuration

    //! Level, as a percentage of empty buffer slots, at which to call Source::freeHighMark().
    //! Trigger condition is when number of free buffer slots goes above this level.
    //! In range [0.0, 1.0)
    void setFreeHighMark(double level);

    // up-stream interface (putting data into FIFO)
    //! Mark subscription as "open" with the associated structure type.
    void open(const epics::pvData::StructureConstPtr& type);
    //! Abnormal closure (eg. due to upstream dis-connection)
    void close();
    //! Successful closure (eg. RDB query done)
    void finish();
    //! Consume a free slot if available. otherwise ...
    //! if !force take no action and return false.
    //! if force then attempt to allocate and fill a new slot, then return false.
    //!   The extra slot will be free'd after it is consumed.
    bool tryPost(const pvData::PVStructure& value,
                 const epics::pvData::BitSet& changed,
                 const epics::pvData::BitSet& overrun = epics::pvData::BitSet(),
                 bool force =false);
    //! Consume a free slot if available, otherwise squash with most recent
    void post(const pvData::PVStructure& value,
              const epics::pvData::BitSet& changed,
              const epics::pvData::BitSet& overrun = epics::pvData::BitSet());
    //! Call after calling any other upstream interface methods (open()/close()/finish()/post()/...)
    //! when no upstream mutexes are locked.
    //! Do not call from Source::freeHighMark().  This is done automatically.
    //! Call any MonitorRequester methods.
    void notify();

    // down-stream interface (taking data from FIFO)
    virtual epics::pvData::Status start() OVERRIDE FINAL;
    virtual epics::pvData::Status stop() OVERRIDE FINAL;
    virtual MonitorElementPtr poll() OVERRIDE FINAL;
    virtual void release(MonitorElementPtr const & monitorElement) OVERRIDE FINAL; // may call Source::freeHighMark()
    virtual void getStats(Stats& s) const OVERRIDE FINAL;
    virtual void reportRemoteQueueStatus(epics::pvData::int32 freeElements) OVERRIDE FINAL;

    //! Number of unused FIFO slots at this moment, which may changed in the next.
    size_t freeCount() const;
private:
    size_t _freeCount() const;

    friend void providerRegInit(void*);
    static size_t num_instances;

    // const after ctor
    Config conf;

    // locking here is complicated...
    // our entry points which make callbacks are:
    //   notify() -> MonitorRequester::monitorConnect()
    //            -> MonitorRequester::monitorEvent()
    //            -> MonitorRequester::unlisten()
    //            -> ChannelBaseRequester::channelDisconnect()
    //   start()  -> MonitorRequester::monitorEvent()
    //   release()                 -> Source::freeHighMark()
    //                             -> notify() -> ...
    //   reportRemoteQueueStatus() -> Source::freeHighMark()
    //                             -> notify() -> ...
    mutable epicsMutex mutex;

    // ownership is archored at the downstream (consumer) end.
    // strong refs are:
    //   downstream -> MonitorFIFO -> Source
    // weak refs are:
    //   MonitorRequester <- MonitorFIFO <- upstream

    // so we expect that downstream will hold a strong ref to us,
    // and we keep a weak ref to downstream's MonitorRequester
    const std::tr1::weak_ptr<MonitorRequester> requester;

    const epics::pvData::PVStructure::const_shared_pointer pvRequest;

    // then we expect to keep a strong ref to upstream (Source)
    // and expect that upstream will have only a weak ref to us.
    const Source::shared_pointer upstream;

    enum state_t {
        Closed, // not open()'d
        Opened, // successful open()
        Error,  // unsuccessful open()
    } state;
    bool pipeline; // const after ctor
    bool running; // start() vs. stop()
    bool finished; // finish() called
    epics::pvData::BitSet scratch, oscratch; // using during post to avoid re-alloc

    bool needConnected;
    bool needEvent;
    bool needUnlisten;
    bool needClosed;

    epics::pvData::Status error; // Set when entering Error state

    size_t freeHighLevel;
    epicsInt32 flowCount;

    epics::pvData::PVRequestMapper mapper;

    typedef std::list<MonitorElementPtr> buffer_t;
    // we allocate one extra buffer element to hold data when post()
    // while all elements poll()'d.  So there will always be one
    // element on either the empty or inuse lists
    buffer_t inuse, empty, returned;
    /* our elements are in one of 4 states
     * Empty - on empty list
     * In Use - on inuse list
     * Polled - Returnedd from poll().  Not tracked
     * Returned - only if pipeline==true, release()'d but not ack'd
     */

    EPICS_NOT_COPYABLE(MonitorFIFO)
};

static inline
std::ostream& operator<<(std::ostream& strm, const MonitorFIFO& fifo) {
    fifo.show(strm);
    return strm;
}

}}

namespace epics { namespace pvData {

using epics::pvAccess::MonitorElement;
using epics::pvAccess::MonitorElementPtr;
using epics::pvAccess::MonitorElementPtrArray;
using epics::pvAccess::Monitor;
using epics::pvAccess::MonitorPtr;
}}

#endif  /* MONITOR_H */
