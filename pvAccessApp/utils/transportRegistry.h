/*
 * transportRegistry.h
 */

#ifndef TRANSPORTREGISTRY_H
#define TRANSPORTREGISTRY_H

#include <map>
#include <vector>
#include <iostream>

#include <osiSock.h>

#include <lock.h>
#include <pvType.h>
#include <epicsException.h>
#include <remote.h>
#include "inetAddressUtil.h"

namespace epics { namespace pvAccess {

class Transport;
//TODO if unordered map is used instead of map we can use sockAddrAreIdentical routine from osiSock.h

typedef std::map<const epics::pvData::int16,Transport*> prioritiesMap_t;
typedef std::map<const osiSockAddr*,prioritiesMap_t*,comp_osiSockAddrPtr> transportsMap_t;
typedef std::vector<Transport*> allTransports_t;

	class TransportRegistry {
	public:
		TransportRegistry();
		virtual ~TransportRegistry();

		void put(Transport* transport);
		Transport* get(const epics::pvData::String type, const osiSockAddr* address, const epics::pvData::int16 priority);
		Transport** get(const epics::pvData::String type, const osiSockAddr* address, epics::pvData::int32& size);
		Transport* remove(Transport* transport);
		void clear();
		epics::pvData::int32 numberOfActiveTransports();
		Transport** toArray(const epics::pvData::String type, epics::pvData::int32& size);
		Transport** toArray(epics::pvData::int32& size);

	private:
		transportsMap_t _transports;
		transportsMap_t::iterator _transportsIter;
		prioritiesMap_t::iterator _prioritiesIter;
		allTransports_t _allTransports;
		allTransports_t::iterator _allTransportsIter;
		epics::pvData::Mutex _mutex;
	};

}}

#endif  /* INTROSPECTIONREGISTRY_H */
