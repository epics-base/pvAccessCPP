/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef TRANSPORTREGISTRY_H
#define TRANSPORTREGISTRY_H

#include <map>
#include <vector>
#include <list>
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
    class Reservation;
private:
    struct Key {
        osiSockAddr addr;
        epics::pvData::int16 prio;
        Key(const osiSockAddr& a, epics::pvData::int16 p) :addr(a), prio(p) {}
        bool operator<(const Key& o) const;
    };

    typedef std::map<Key, Transport::shared_pointer> transports_t;
    typedef std::map<Key, std::tr1::shared_ptr<epics::pvData::Mutex> > locks_t;

public:
    POINTER_DEFINITIONS(TransportRegistry);

    typedef std::vector<Transport::shared_pointer> transportVector_t;

    class Reservation {
        TransportRegistry* const owner;
        const Key key;
        std::tr1::shared_ptr<epics::pvData::Mutex> mutex;
    public:

        // ctor blocks until no concurrent connect() in progress (success or failure)
        Reservation(TransportRegistry *owner, const osiSockAddr& address, epics::pvData::int16 prio);
        ~Reservation();
    };

    TransportRegistry() {}
    ~TransportRegistry();

    Transport::shared_pointer get(const osiSockAddr& address, epics::pvData::int16 prio);
    void install(const Transport::shared_pointer& ptr);
    Transport::shared_pointer remove(Transport::shared_pointer const & transport);
    void clear();
    size_t size();

    // optimized to avoid reallocation, adds to array
    void toArray(transportVector_t & transportArray, const osiSockAddr *dest=0);

private:
    transports_t transports;
    // per destination mutex to serialize concurrent connect() attempts
    locks_t locks;

    epics::pvData::Mutex _mutex;
};

}
}

#endif  /* INTROSPECTIONREGISTRY_H */
