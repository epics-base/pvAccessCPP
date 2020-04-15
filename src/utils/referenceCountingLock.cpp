/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define epicsExportSharedSymbols
#include <pv/referenceCountingLock.h>

namespace epics {
namespace pvAccess {

using namespace epics::pvData;

ReferenceCountingLock::ReferenceCountingLock(): _references(1)
{
    /*  pthread_mutexattr_t mutexAttribute;
        int32 retval = pthread_mutexattr_init(&mutexAttribute);
        if(retval != 0)
        {
            //string errMsg = "Error: pthread_mutexattr_init failed: " + string(strerror(retval));
            assert(false);
        }
        retval = pthread_mutexattr_settype(&mutexAttribute, PTHREAD_MUTEX_RECURSIVE);
        if(retval == 0)
        {
            retval = pthread_mutex_init(_mutex, &mutexAttribute);
            if(retval != 0)
            {
                //string errMsg = "Error: pthread_mutex_init failed: " + string(strerror(retval));
                assert(false);
            }
        }
        else
        {
            //string errMsg = "Error: pthread_mutexattr_settype failed: " + string(strerror(retval));
            assert(false);
        }

        pthread_mutexattr_destroy(&mutexAttribute);*/
}

ReferenceCountingLock::~ReferenceCountingLock()
{
//  pthread_mutex_destroy(_mutex);
}

bool ReferenceCountingLock::acquire(int64 /*msecs*/)
{
    _mutex.lock();
    return true;
    /*  struct timespec deltatime;
        if(msecs > 0)
        {
            deltatime.tv_sec = msecs / 1000;
            deltatime.tv_nsec = (msecs % 1000) * 1000;
        }
        else
        {
            deltatime.tv_sec = 0;
            deltatime.tv_nsec = 0;
        }

        int32 retval = pthread_mutex_timedlock(_mutex, &deltatime);
        if(retval == 0)
        {
            return true;
        }
        return false;
    */
}

void ReferenceCountingLock::release()
{
    _mutex.unlock();
    /*  int retval = pthread_mutex_unlock(_mutex);
        if(retval != 0)
        {
            //string errMsg = "Error: pthread_mutex_unlock failed: "  + string(strerror(retval));
            //TODO do something?
        }*/
}

// TODO use atomic primitive impl.
int ReferenceCountingLock::increment()
{
    Lock guard(_countMutex);
    ++_references;
    return _references;
}

int ReferenceCountingLock::decrement()
{
    Lock guard(_countMutex);
    --_references;
    return _references;
}

}
}
