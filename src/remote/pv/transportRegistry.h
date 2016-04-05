/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef TRANSPORTREGISTRY_H
#define TRANSPORTREGISTRY_H

#include <map>
#include <vector>
#include <iostream>

#ifdef epicsExportSharedSymbols
#   define transportRegistryEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <osiSock.h>

#include <pv/lock.h>
#include <pv/pvType.h>
#include <pv/epicsException.h>
#include <pv/sharedPtr.h>

#ifdef transportRegistryEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef transportRegistryEpicsExportSharedSymbols
#endif

#include <pv/remote.h>
#include <pv/inetAddressUtil.h>

namespace epics {
namespace pvAccess {

class TransportRegistry {
public:
    typedef std::tr1::shared_ptr<TransportRegistry> shared_pointer;
    typedef std::tr1::shared_ptr<const TransportRegistry> const_shared_pointer;

    typedef std::vector<Transport::shared_pointer> transportVector_t;

    TransportRegistry();
    virtual ~TransportRegistry();

    void put(Transport::shared_pointer const & transport);
    Transport::shared_pointer get(std::string const & type, const osiSockAddr* address, const epics::pvData::int16 priority);
    std::auto_ptr<transportVector_t> get(std::string const & type, const osiSockAddr* address);
    Transport::shared_pointer remove(Transport::shared_pointer const & transport);
    void clear();
    epics::pvData::int32 numberOfActiveTransports();

    // TODO note type not supported
    std::auto_ptr<transportVector_t> toArray(std::string const & type);
    std::auto_ptr<transportVector_t> toArray();
    // optimized to avoid reallocation, adds to array
    void toArray(transportVector_t & transportArray);

private:
    //TODO if unordered map is used instead of map we can use sockAddrAreIdentical routine from osiSock.h
    // NOTE: pointers are used to osiSockAddr (to save memory), since it guaranteed that their reference is valid as long as Transport
    typedef std::map<const epics::pvData::int16,Transport::shared_pointer> prioritiesMap_t;
    typedef std::tr1::shared_ptr<prioritiesMap_t> prioritiesMapSharedPtr_t;
    typedef std::map<const osiSockAddr*,prioritiesMapSharedPtr_t,comp_osiSockAddrPtr> transportsMap_t;

    transportsMap_t _transports;
    epics::pvData::int32 _transportCount;
    epics::pvData::Mutex _mutex;
};

}
}

#endif  /* INTROSPECTIONREGISTRY_H */
