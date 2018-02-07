/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define epicsExportSharedSymbols
#include <pv/transportRegistry.h>
#include <pv/logger.h>

namespace pvd = epics::pvData;

namespace epics {
namespace pvAccess {


bool TransportRegistry::Key::operator<(const Key& o) const
{
    if(addr.sa.sa_family<o.addr.sa.sa_family)
        return true;
    if(addr.sa.sa_family>o.addr.sa.sa_family)
        return false;
    if(addr.ia.sin_addr.s_addr<o.addr.ia.sin_addr.s_addr)
        return true;
    if(addr.ia.sin_addr.s_addr>o.addr.ia.sin_addr.s_addr)
        return false;
    if(addr.ia.sin_port<o.addr.ia.sin_port)
        return true;
    if(addr.ia.sin_port>o.addr.ia.sin_port)
        return false;
    if(prio<o.prio)
        return true;
    return false;
}

TransportRegistry::Reservation::Reservation(TransportRegistry *owner,
                                            const osiSockAddr& address,
                                            pvd::int16 prio)
    :owner(owner)
    ,key(address, prio)
{
    {
        pvd::Lock G(owner->_mutex);

        std::tr1::shared_ptr<pvd::Mutex>& lock = owner->locks[key]; // fetch or alloc
        if(!lock)
            lock.reset(new pvd::Mutex());

        mutex = lock;
    }

    mutex->lock();
}

TransportRegistry::Reservation::~Reservation()
{
    mutex->unlock();

    pvd::Lock G(owner->_mutex);

    assert(mutex.use_count()>=2);

    if(mutex.use_count()==2) {
        // no other concurrent connect(), so can drop this lock
        owner->locks.erase(key);
    }

    assert(mutex.use_count()==1);
}

TransportRegistry::~TransportRegistry()
{
    pvd::Lock G(_mutex);
    if(!transports.empty())
        LOG(logLevelWarn, "TransportRegistry destroyed while not empty");
}

Transport::shared_pointer TransportRegistry::get(const osiSockAddr& address, epics::pvData::int16 prio)
{
    const Key key(address, prio);

    pvd::Lock G(_mutex);

    transports_t::iterator it(transports.find(key));
    if(it!=transports.end()) {
        return it->second;
    }
    return Transport::shared_pointer();
}

void TransportRegistry::install(const Transport::shared_pointer& ptr)
{
    const Key key(ptr->getRemoteAddress(), ptr->getPriority());

    pvd::Lock G(_mutex);

    std::pair<transports_t::iterator, bool> itpair(transports.insert(std::make_pair(key, ptr)));
    if(!itpair.second)
        THROW_EXCEPTION2(std::logic_error, "Refuse to insert dup");
}

Transport::shared_pointer TransportRegistry::remove(Transport::shared_pointer const & transport)
{
    assert(!!transport);
    const Key key(transport->getRemoteAddress(), transport->getPriority());
    Transport::shared_pointer ret;

    pvd::Lock guard(_mutex);
    transports_t::iterator it(transports.find(key));
    if(it!=transports.end()) {
        ret.swap(it->second);
        transports.erase(it);
    }
    return ret;
}

#define LEAK_CHECK(PTR, NAME) if((PTR) && !(PTR).unique()) { std::cerr<<"Leaking Transport " NAME " use_count="<<(PTR).use_count()<<"\n"<<show_referrers(PTR, false);}

void TransportRegistry::clear()
{
    transports_t temp;
    {
        pvd::Lock guard(_mutex);
        transports.swap(temp);
    }

    if(temp.empty())
        return;

    LOG(logLevelDebug, "Context still has %zu transport(s) active and closing...", temp.size());

    for(transports_t::iterator it(temp.begin()), end(temp.end());
        it != end; ++it)
    {
        it->second->close();
    }

    for(transports_t::iterator it(temp.begin()), end(temp.end());
        it != end; ++it)
    {
        const Transport::shared_pointer& transport = it->second;
        transport->waitJoin();
        LEAK_CHECK(transport, "tcp transport")
        if(!transport.unique()) {
            LOG(logLevelError, "Closed transport %s still has use_count=%u",
                transport->getRemoteName().c_str(),
                (unsigned)transport.use_count());
        }
    }
}

size_t TransportRegistry::size()
{
    pvd::Lock guard(_mutex);
    return transports.size();
}

void TransportRegistry::toArray(transportVector_t & transportArray, const osiSockAddr *dest)
{
    pvd::Lock guard(_mutex);

    transportArray.reserve(transportArray.size() + transports.size());

    for(transports_t::const_iterator it(transports.begin()), end(transports.end());
        it != end; ++it)
    {
        const Key& key = it->first;
        const Transport::shared_pointer& tr = it->second;

        if(!dest || sockAddrAreIdentical(dest, &key.addr))
            transportArray.push_back(tr);
    }
}

}
}

