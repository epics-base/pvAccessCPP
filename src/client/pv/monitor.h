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

#include <pv/status.h>
#include <pv/destroyable.h>
#include <pv/pvData.h>
#include <pv/sharedPtr.h>
#include <pv/bitSet.h>
#include <pv/requester.h>

#include <shareLib.h>

namespace epics { namespace pvAccess {

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
    MonitorElement(epics::pvData::PVStructurePtr const & pvStructurePtr)
    : pvStructurePtr(pvStructurePtr),
      changedBitSet(epics::pvData::BitSet::create(static_cast<epics::pvData::uint32>(pvStructurePtr->getNumberFields()))),
      overrunBitSet(epics::pvData::BitSet::create(static_cast<epics::pvData::uint32>(pvStructurePtr->getNumberFields())))
    {}
    epics::pvData::PVStructurePtr pvStructurePtr;
    epics::pvData::BitSet::shared_pointer changedBitSet;
    epics::pvData::BitSet::shared_pointer overrunBitSet;
};

/**
 * @brief Monitor changes to fields of a pvStructure.
 *
 * This is used by pvAccess to implement monitors.
 * @author mrk
 */
class epicsShareClass Monitor : public epics::pvData::Destroyable{
    public:
    POINTER_DEFINITIONS(Monitor);
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
};


}}

namespace epics { namespace pvData {

using epics::pvAccess::MonitorElement;
using epics::pvAccess::MonitorElementPtr;
using epics::pvAccess::MonitorElementPtrArray;
using epics::pvAccess::Monitor;
using epics::pvAccess::MonitorPtr;
}}

#endif  /* MONITOR_H */
