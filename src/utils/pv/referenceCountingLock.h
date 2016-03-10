/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef REFERENCECOUNTINGLOCK_H
#define REFERENCECOUNTINGLOCK_H

#ifdef epicsExportSharedSymbols
#   define referenceCountingLockEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/lock.h>
#include <pv/pvType.h>
#include <pv/sharedPtr.h>

#ifdef referenceCountingLockEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef referenceCountingLockEpicsExportSharedSymbols
#endif

namespace epics {
namespace pvAccess {

/**
 * Reference counting mutex implementation w/ deadlock detection.
 * Synchronization helper class used (intended for use) for activation/deactivation synchronization.
 * This class enforces <code>attempt</code> method of acquiring the locks to prevent deadlocks.
 * Class also offers reference counting.
 * (NOTE: automatic lock counting was not implemented due to imperfect usage.)
 *
 */
class ReferenceCountingLock
{
public:
    POINTER_DEFINITIONS(ReferenceCountingLock);

    /**
     * Constructor of <code>ReferenceCountingLock</code>.
     * After construction lock is free and reference count equals <code>1</code>.
     */
    ReferenceCountingLock();
    /**
     * Destructor of <code>ReferenceCountingLock</code>.
     */
    virtual ~ReferenceCountingLock();
    /**
     * Attempt to acquire lock.
     *
     * NOTE: Argument msecs is currently not supported due to
     * Darwin OS not supporting pthread_mutex_timedlock. May be changed in the future.
     *
     * @param	msecs	the number of milleseconds to wait.
     * 					An argument less than or equal to zero means not to wait at all.
     *
     * @return	<code>true</code> if acquired, <code>false</code> otherwise.
     * 			NOTE: currently this routine always returns true. Look above for explanation.
     *
     */
    bool acquire(epics::pvData::int64 msecs);
    /**
     * Release previously acquired lock.
     */
    void release();
    /**
     * Increment number of references.
     *
     * @return number of references.
     */
    int increment();
    /**
     * Decrement number of references.
     *
     * @return number of references.
     */
    int decrement();
private:
    int _references;
    epics::pvData::Mutex _mutex;
    epics::pvData::Mutex _countMutex;
};

}
}

#endif  /* REFERENCECOUNTINGLOCK_H */
