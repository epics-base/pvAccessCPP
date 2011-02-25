/*
 * referencedTransportSender.cpp
 */

#include "referencedTransportSender.h"

using namespace epics::pvData;

namespace epics { namespace pvAccess {

void ReferencedTransportSender::release()
{
	_refMutex.lock();
	_refCount--;
	_refMutex.unlock();
	if (_refCount == 0)
	{
		delete this;
	}
}

void ReferencedTransportSender::acquire()
{
    Lock guard(_refMutex);
    _refCount++;
}

}
}
