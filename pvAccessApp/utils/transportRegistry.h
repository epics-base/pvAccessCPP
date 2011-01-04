/*
 * transportRegistry.h
 */

#ifndef TRANSPORTREGISTRY_H
#define TRANSPORTREGISTRY_H

#include <map>
#include <vector>
#include <iostream>
#include <epicsMutex.h>

#include <osiSock.h>

#include "lock.h"
#include "pvType.h"
#include "epicsException.h"
#include "inetAddressUtil.h"
#include "remote.h"

using namespace epics::pvData;
using namespace std;

namespace epics { namespace pvAccess {

class Transport;
//TODO if unordered map is used instead of map we can use sockAddrAreIdentical routine from osiSock.h
struct comp_osiSockAddr
{
	bool operator()(osiSockAddr const *a, osiSockAddr const *b)
	{
		if (a->sa.sa_family < b->sa.sa_family) return true;
		if ((a->sa.sa_family == b->sa.sa_family) && (a->ia.sin_addr.s_addr < b->ia.sin_addr.s_addr )) return true;
		if ((a->sa.sa_family == b->sa.sa_family) && (a->ia.sin_addr.s_addr == b->ia.sin_addr.s_addr ) && ( a->ia.sin_port < b->ia.sin_port )) return true;
		return false;
	}
};

typedef std::map<const int16,Transport*> prioritiesMap_t;
typedef std::map<const osiSockAddr*,prioritiesMap_t*,comp_osiSockAddr> transportsMap_t;
typedef std::vector<Transport*> allTransports_t;

	class TransportRegistry {
	public:
		TransportRegistry();
		virtual ~TransportRegistry();

		void put(Transport* transport);
		Transport* get(const string type, const osiSockAddr* address, const int16 priority);
		Transport** get(const string type, const osiSockAddr* address, int32& size);
		Transport* remove(Transport* transport);
		void clear();
		int32 numberOfActiveTransports();
		Transport** toArray(const string type, int32& size);
		Transport** toArray(int32& size);

	private:
		transportsMap_t _transports;
		transportsMap_t::iterator _transportsIter;
		prioritiesMap_t::iterator _prioritiesIter;
		allTransports_t _allTransports;
		allTransports_t::iterator _allTransportsIter;
		Mutex _mutex;
	};

}}

#endif  /* INTROSPECTIONREGISTRY_H */
