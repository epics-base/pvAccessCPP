/*
 * referenceCountingLock.h
 */

#ifndef REFERENCECOUNTINGLOCK_H
#define REFERENCECOUNTINGLOCK_H

#include <map>
#include <iostream>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include <lock.h>
#include <pvType.h>
#include <epicsAssert.h>

using namespace std;
using namespace epics::pvData;

namespace epics { namespace pvAccess {

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
	bool acquire(int64 msecs);
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
	Mutex _mutex;
	Mutex _countMutex;
	//pthread_mutex_t _mutex;

};

}}

#endif  /* REFERENCECOUNTINGLOCK_H */
