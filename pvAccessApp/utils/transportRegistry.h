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


typedef std::map<const int16,Transport*> prioritiesMap_t;
typedef std::map<const int32,prioritiesMap_t*> transportsMap_t;
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
