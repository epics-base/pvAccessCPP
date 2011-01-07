/*
 * namedLockPattern.cpp
 */

#include "referenceCountingLock.h"

namespace epics { namespace pvAccess {

ReferenceCountingLock::ReferenceCountingLock(): _references(1)
{
	pthread_mutexattr_t mutexAttribute;
	int32 retval = pthread_mutexattr_init(&mutexAttribute);
	if(retval != 0)
	{
		//string errMsg = "Error: pthread_mutexattr_init failed: " + string(strerror(retval));
		assert(false);
	}
	retval = pthread_mutexattr_settype(&mutexAttribute, PTHREAD_MUTEX_RECURSIVE);
	if(retval == 0)
	{
		retval = pthread_mutex_init(&_mutex, &mutexAttribute);
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

	pthread_mutexattr_destroy(&mutexAttribute);
}

ReferenceCountingLock::~ReferenceCountingLock()
{
	pthread_mutex_destroy(&_mutex);
}

bool ReferenceCountingLock::acquire(int64 msecs)
{
#ifdef darwin
    // timedlock not supported by Darwin OS
 	return (pthread_mutex_lock(&_mutex) == 0);
#else
	struct timespec deltatime;
	deltatime.tv_sec = msecs / 1000;
	deltatime.tv_nsec = (msecs % 1000) * 1000;

	int32 retval = pthread_mutex_timedlock(&_mutex, &deltatime);
	if(retval == 0)
	{
		return true;
	}
	return false;
#endif
}

void ReferenceCountingLock::release()
{
	int retval = pthread_mutex_unlock(&_mutex);
	if(retval != 0)
	{
		//string errMsg = "Error: pthread_mutex_unlock failed: "  + string(strerror(retval));
		//TODO do something?
	}
}

int ReferenceCountingLock::increment()
{
	//TODO does it really has to be atomic?
	return ++_references;
	// commented because linking depends on specific version of glibc library
	// on i386 target
	//return __sync_add_and_fetch(&_references,1);
}

int ReferenceCountingLock::decrement()
{
	//TODO does it really has to be atomic?
	return --_references;
    // commented because linking depends on specific version of glibc library
    // on i386 target
	//return __sync_sub_and_fetch(&_references,1);
}

}}

