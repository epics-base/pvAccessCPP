/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef NAMEDLOCKPATTERN_H
#define NAMEDLOCKPATTERN_H

#include <map>
#include <iostream>

#ifdef epicsExportSharedSymbols
#   define namedLockPatternEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <compilerDependencies.h>

#include <pv/lock.h>
#include <pv/pvType.h>
#include <pv/sharedPtr.h>

#ifdef namedLockPatternEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef namedLockPatternEpicsExportSharedSymbols
#endif

#include <pv/referenceCountingLock.h>

// TODO implement using smart pointers

namespace epics {
namespace pvAccess {

/**
 * NamedLockPattern
 */
template <class Key, class Compare = std::less<Key> >
class EPICS_DEPRECATED NamedLockPattern
{
public:
    /**
     * Constructor.
     */
    NamedLockPattern() {};
    /**
     * Destructor.
     */
    virtual ~NamedLockPattern() {};
    /**
     * Acquire synchronization lock for named object.
     *
     * NOTE: Argument msecs is currently not supported due to
     * Darwin OS not supporting pthread_mutex_timedlock. May be changed in the future.
     *
     * @param	name	name of the object whose lock to acquire.
     * @param	msec	the number of milleseconds to wait.
     * 					An argument less than or equal to zero means not to wait at all.
     * @return	<code>true</code> if acquired, <code>false</code> othwerwise.
     * 			NOTE: currently this routine always returns true. Look above for explanation.
     */
    bool acquireSynchronizationObject(const Key& name, const epics::pvData::int64 msec);
    /**
     * Release synchronization lock for named object.
     * @param	name	name of the object whose lock to release.
     */
    void releaseSynchronizationObject(const Key& name);
private:
    epics::pvData::Mutex _mutex;
    std::map<const Key,ReferenceCountingLock::shared_pointer,Compare> _namedLocks;
    typename std::map<const Key,ReferenceCountingLock::shared_pointer,Compare>::iterator _namedLocksIter;

    /**
     * Release synchronization lock for named object.
     * @param	name	name of the object whose lock to release.
     * @param	release	set to <code>false</code> if there is no need to call release
     * 					on synchronization lock.
     */
    void releaseSynchronizationObject(const Key& name,const bool release);
};

template <class Key, class Compare>
bool NamedLockPattern<Key,Compare>::acquireSynchronizationObject(const Key& name, const epics::pvData::int64 msec)
{
    ReferenceCountingLock::shared_pointer lock;
    {   //due to guard
        epics::pvData::Lock guard(_mutex);

        _namedLocksIter = _namedLocks.find(name);
        // get synchronization object

        // none is found, create and return new one
        // increment references
        if(_namedLocksIter == _namedLocks.end())
        {
            lock.reset(new ReferenceCountingLock());
            _namedLocks[name] = lock;
        }
        else
        {
            lock = _namedLocksIter->second;
            lock->increment();
        }
    } // end of guarded area

    bool success = lock->acquire(msec);

    if(!success)
    {
        releaseSynchronizationObject(name, false);
    }

    return success;
}

template <class Key, class Compare>
void NamedLockPattern<Key,Compare>::releaseSynchronizationObject(const Key& name)
{
    releaseSynchronizationObject(name, true);
}

template <class Key, class Compare>
void NamedLockPattern<Key,Compare>::releaseSynchronizationObject(const Key& name,const bool release)
{
    epics::pvData::Lock guard(_mutex);
    ReferenceCountingLock::shared_pointer lock;
    _namedLocksIter = _namedLocks.find(name);

    // release lock
    if (_namedLocksIter != _namedLocks.end())
    {
        lock = _namedLocksIter->second;

        // release the lock
        if (release)
        {
            lock->release();
        }

        // if there only one current lock exists
        // remove it from the map
        if (lock->decrement() <= 0)
        {
            _namedLocks.erase(_namedLocksIter);
        }
    }
}

template <class Key, class Compare>
class NamedLock : private epics::pvData::NoDefaultMethods
{
public:
    NamedLock(NamedLockPattern<Key,Compare>* namedLockPattern): _namedLockPattern(namedLockPattern) {}
    bool acquireSynchronizationObject(const Key& name, const epics::pvData::int64 msec) {
        _name = name;
        return _namedLockPattern->acquireSynchronizationObject(name,msec);
    }
    ~NamedLock() {
        _namedLockPattern->releaseSynchronizationObject(_name);
    }
private:
    Key _name;
    NamedLockPattern<Key,Compare>* _namedLockPattern;
};

}
}

#endif  /* NAMEDLOCKPATTERN_H */
