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

#ifdef epicsExportSharedSymbols
#   define monitorEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/status.h>
#include <pv/pvData.h>
#include <pv/sharedPtr.h>
#include <pv/bitSet.h>

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
    MonitorElement(){}
    MonitorElement(epics::pvData::PVStructurePtr const & pvStructurePtr);
    const epics::pvData::PVStructurePtr pvStructurePtr;
    const epics::pvData::BitSet::shared_pointer changedBitSet;
    const epics::pvData::BitSet::shared_pointer overrunBitSet;
    // info to assist monitor debugging
    enum state_t {
        Free,   //!< data invalid.  eg. on internal free list
        Queued, //!< data valid.  Owned by Monitor.  Waiting for Monitor::poll()
        InUse   //!< data valid.  Owned by MonitorRequester.  Waiting for Monitor::release()
    } state;

    /** A smart pointer to extract a MonitorElement from a Monitor queue
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

class MonitorElement::Ref
{
    Monitor* mon;
    MonitorElementPtr elem;
public:
    Ref() :mon(0), elem() {}
    Ref(Monitor& M) :mon(&M), elem(mon->poll()) {}
    Ref(const Monitor::shared_pointer& M) :mon(M.get()), elem(mon->poll()) {}
    ~Ref() { reset(); }
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
};

#if __cplusplus>=201103L
// used by c++11 for-range
inline MonitorElement::Ref begin(Monitor& mon) { return MonitorElement::Ref(mon); }
inline MonitorElement::Ref end(Monitor& mon) { return MonitorElement::Ref(); }
#endif // __cplusplus<201103L

}}

namespace epics { namespace pvData {

using epics::pvAccess::MonitorElement;
using epics::pvAccess::MonitorElementPtr;
using epics::pvAccess::MonitorElementPtrArray;
using epics::pvAccess::Monitor;
using epics::pvAccess::MonitorPtr;
}}

#endif  /* MONITOR_H */
