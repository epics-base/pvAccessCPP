/*
 * namedLockPatternTest.cpp
 *
 */

#include <pv/namedLockPattern.h>
#include <pv/inetAddressUtil.h>
#include <pv/status.h>
#include <pv/CDRMonitor.h>


#include <epicsAssert.h>
#include <epicsExit.h>
#include <osiSock.h>

#include <iostream>
#include <string.h>

using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace std;

void testIntLockPattern()
{
	int64 timeout = 100;
	NamedLockPattern<int> namedLockPattern;
	int name1 = 1;
	assert(namedLockPattern.acquireSynchronizationObject(name1,timeout));
	assert(namedLockPattern.acquireSynchronizationObject(name1,timeout));
	namedLockPattern.releaseSynchronizationObject(name1);
	namedLockPattern.releaseSynchronizationObject(name1);
	int name2 = 2;
	assert(namedLockPattern.acquireSynchronizationObject(name2,timeout));
	namedLockPattern.releaseSynchronizationObject(name2);
}

void testIntPtrLockPattern()
{
	int64 timeout = 100;
	NamedLockPattern<int*> namedLockPattern;
	int name1 = 1;
	assert(namedLockPattern.acquireSynchronizationObject(&name1,timeout));
	assert(namedLockPattern.acquireSynchronizationObject(&name1,timeout));
	namedLockPattern.releaseSynchronizationObject(&name1);
	namedLockPattern.releaseSynchronizationObject(&name1);
	int name2 = 2;
	assert(namedLockPattern.acquireSynchronizationObject(&name2,timeout));
	namedLockPattern.releaseSynchronizationObject(&name2);
}

struct cmp_str
{
	bool operator()(char const *a, char const *b)
	{
		return strcmp(a, b) < 0;
	}
};

void testCharPtrLockPattern()
{
	int64 timeout = 100;
	NamedLockPattern<const char*,cmp_str> namedLockPattern;
	string name1 = "lojze";
	assert(namedLockPattern.acquireSynchronizationObject(name1.c_str(),timeout));
	assert(namedLockPattern.acquireSynchronizationObject(name1.c_str(),timeout));
	namedLockPattern.releaseSynchronizationObject(name1.c_str());
	namedLockPattern.releaseSynchronizationObject(name1.c_str());
	string name2 = "francka";
	assert(namedLockPattern.acquireSynchronizationObject(name2.c_str(),timeout));
	namedLockPattern.releaseSynchronizationObject(name2.c_str());
}

void testOsiSockAddrLockPattern()
{
	int64 timeout = 10000;
	NamedLockPattern<const osiSockAddr*,comp_osiSockAddrPtr> namedLockPattern;
	osiSockAddr name1;
	name1.ia.sin_addr.s_addr = 1;
	name1.ia.sin_port = 1;
	name1.ia.sin_family = AF_INET;

	assert(namedLockPattern.acquireSynchronizationObject(&name1,timeout));
	assert(namedLockPattern.acquireSynchronizationObject(&name1,timeout));


	osiSockAddr name2;
	name2.ia.sin_addr.s_addr = 1;
	name2.ia.sin_port = 1;
	name2.ia.sin_family = AF_INET;
	assert(namedLockPattern.acquireSynchronizationObject(&name2,timeout));
	assert(namedLockPattern.acquireSynchronizationObject(&name2,timeout));


	namedLockPattern.releaseSynchronizationObject(&name1);
	namedLockPattern.releaseSynchronizationObject(&name1);
	namedLockPattern.releaseSynchronizationObject(&name2);
	namedLockPattern.releaseSynchronizationObject(&name2);

	osiSockAddr name3;
	name3.ia.sin_addr.s_addr = 1;
	name3.ia.sin_port = 1;
	name3.ia.sin_family = AF_INET;
	NamedLock<const osiSockAddr*,comp_osiSockAddrPtr> namedGuard(&namedLockPattern);
	assert(namedGuard.acquireSynchronizationObject(&name3,timeout));
}

void testOsiSockAddrWithPtrKeyLockPattern()
{
	int64 timeout = 10000;
	NamedLockPattern<const osiSockAddr*,comp_osiSockAddrPtr> namedLockPattern;
	osiSockAddr* name1 = new osiSockAddr;
	name1->ia.sin_addr.s_addr = 1;
	name1->ia.sin_port = 1;
	name1->ia.sin_family = AF_INET;
	assert(namedLockPattern.acquireSynchronizationObject(name1,timeout));
	assert(namedLockPattern.acquireSynchronizationObject(name1,timeout));
	namedLockPattern.releaseSynchronizationObject(name1);
	namedLockPattern.releaseSynchronizationObject(name1);
	delete name1;
}

void* testWorker1(void* p)
{
	int32 timeout = 1000;
	const int32 max = 1000;
	NamedLockPattern<osiSockAddr,comp_osiSockAddr>* namedLockPattern = (NamedLockPattern<osiSockAddr,comp_osiSockAddr>*)p;

	for(int32 i = 0 ; i < max; i = i +2)
	{
		osiSockAddr addr;
		addr.ia.sin_addr.s_addr = i;
		addr.ia.sin_port = i;
		addr.ia.sin_family = AF_INET;
		NamedLock<osiSockAddr,comp_osiSockAddr> namedGuard(namedLockPattern);
		assert(namedGuard.acquireSynchronizationObject(addr,timeout));
		usleep(1);
	}

	//this one takes a lock, thread 2 will be slower and will get timeout
	{ //due to namedGuard
		osiSockAddr addr;
		addr.ia.sin_addr.s_addr = 1;
		addr.ia.sin_port = 1;
		addr.ia.sin_family = AF_INET;
		NamedLock<osiSockAddr,comp_osiSockAddr> namedGuard(namedLockPattern);
		assert(namedGuard.acquireSynchronizationObject(addr,timeout));
		sleep(5);
	}

	return NULL;
}


void* testWorker2(void* p)
{
	int32 timeout = 1000;
	const int32 max = 1000;
	NamedLockPattern<osiSockAddr,comp_osiSockAddr>* namedLockPattern = (NamedLockPattern<osiSockAddr,comp_osiSockAddr>*)p;

	for(int32 i = 1 ; i < max; i = i + 2)
	{
		osiSockAddr addr;
		addr.ia.sin_addr.s_addr = i;
		addr.ia.sin_port = i;
		addr.ia.sin_family = AF_INET;
		NamedLock<osiSockAddr,comp_osiSockAddr> namedGuard(namedLockPattern);
		assert(namedGuard.acquireSynchronizationObject(addr,timeout));
		usleep(1);
	}

	//this thread sleeps a while and gets timeout on lock
	{
		sleep(1);
		osiSockAddr addr;
		addr.ia.sin_addr.s_addr = 1;
		addr.ia.sin_port = 1;
		addr.ia.sin_family = AF_INET;
		NamedLock<osiSockAddr,comp_osiSockAddr> namedGuard(namedLockPattern);
		//TODO swap next two lines if timed lock used
		//assert(!namedGuard.acquireSynchronizationObject(addr,timeout));
		assert(namedGuard.acquireSynchronizationObject(addr,timeout));
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	testIntLockPattern();
	testIntPtrLockPattern();
	testCharPtrLockPattern();
	testOsiSockAddrLockPattern();
	testOsiSockAddrWithPtrKeyLockPattern();
	pthread_t _worker1Id;
	pthread_t _worker2Id;

	NamedLockPattern<osiSockAddr,comp_osiSockAddr> namedLockPattern;

	//create two threads
	int32 retval = pthread_create(&_worker1Id, NULL, testWorker1, &namedLockPattern);
	if(retval != 0)
	{
		assert(true);
	}

	retval = pthread_create(&_worker2Id, NULL, testWorker2, &namedLockPattern);
	if(retval != 0)
	{
		assert(true);
	}

	//wait for threads
	retval = pthread_join(_worker1Id, NULL);
	if(retval != 0)
	{
		assert(true);
	}

	retval = pthread_join(_worker2Id, NULL);
	if(retval != 0)
	{
		assert(true);
	}

	epicsExitCallAtExits();
	CDRMonitor::get().show(stdout);

	return 0;
}


