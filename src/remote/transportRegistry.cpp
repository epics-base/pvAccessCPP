/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define epicsExportSharedSymbols
#include <pv/transportRegistry.h>

using namespace epics::pvData;

namespace epics {
namespace pvAccess {

TransportRegistry::TransportRegistry(): _transports(), _transportCount(0), _mutex()
{

}

TransportRegistry::~TransportRegistry()
{
}

void TransportRegistry::put(Transport::shared_pointer const & transport)
{
    Lock guard(_mutex);
    //const string type = transport.getType();
    const int16 priority = transport->getPriority();
    const osiSockAddr* address = transport->getRemoteAddress();

    transportsMap_t::iterator transportsIter = _transports.find(address);
    prioritiesMapSharedPtr_t priorities;
    if(transportsIter == _transports.end())
    {
        priorities.reset(new prioritiesMap_t());
        _transports[address] = priorities;
        _transportCount++;
    }
    else
    {
        priorities = transportsIter->second;
        prioritiesMap_t::iterator prioritiesIter = priorities->find(priority);
        if(prioritiesIter == priorities->end()) //only increase transportCount if not replacing
        {
            _transportCount++;
        }
    }
    (*priorities)[priority] = transport;
}

Transport::shared_pointer TransportRegistry::get(std::string const & /*type*/, const osiSockAddr* address, const int16 priority)
{
    Lock guard(_mutex);
    transportsMap_t::iterator transportsIter = _transports.find(address);
    if(transportsIter != _transports.end())
    {
        prioritiesMapSharedPtr_t priorities = transportsIter->second;
        prioritiesMap_t::iterator prioritiesIter = priorities->find(priority);
        if(prioritiesIter != priorities->end())
        {
            return prioritiesIter->second;
        }
    }
    return Transport::shared_pointer();
}

void
TransportRegistry::get(std::string const & /*type*/, const osiSockAddr* address, transportVector_t& output)
{
    Lock guard(_mutex);
    transportsMap_t::iterator transportsIter = _transports.find(address);
    if(transportsIter != _transports.end())
    {
        prioritiesMapSharedPtr_t& priorities = transportsIter->second;

        size_t i = output.size();
        output.resize(output.size() + priorities->size());

        for(prioritiesMap_t::iterator prioritiesIter = priorities->begin();
                prioritiesIter != priorities->end();
                prioritiesIter++, i++)
        {
            output[i] = prioritiesIter->second;
        }
    }
}

Transport::shared_pointer TransportRegistry::remove(Transport::shared_pointer const & transport)
{
    Lock guard(_mutex);
    const int16 priority = transport->getPriority();
    const osiSockAddr* address = transport->getRemoteAddress();
    Transport::shared_pointer retTransport;
    transportsMap_t::iterator transportsIter = _transports.find(address);
    if(transportsIter != _transports.end())
    {
        prioritiesMapSharedPtr_t priorities = transportsIter->second;
        prioritiesMap_t::iterator prioritiesIter = priorities->find(priority);
        if(prioritiesIter != priorities->end())
        {
            retTransport = prioritiesIter->second;
            priorities->erase(prioritiesIter);
            _transportCount--;
            if(priorities->size() == 0)
            {
                _transports.erase(transportsIter);
            }
        }
    }
    return retTransport;
}

void TransportRegistry::clear()
{
    Lock guard(_mutex);
    _transports.clear();
    _transportCount = 0;
}

int32 TransportRegistry::numberOfActiveTransports()
{
    Lock guard(_mutex);
    return _transportCount;
}

void TransportRegistry::toArray(transportVector_t & transportArray)
{
    Lock guard(_mutex);
    if (_transportCount == 0)
        return;

    transportArray.reserve(transportArray.size() + _transportCount);

    for (transportsMap_t::iterator transportsIter = _transports.begin();
            transportsIter != _transports.end();
            transportsIter++)
    {
        prioritiesMapSharedPtr_t priorities = transportsIter->second;
        for (prioritiesMap_t::iterator prioritiesIter = priorities->begin();
                prioritiesIter != priorities->end();
                prioritiesIter++)
        {
            transportArray.push_back(prioritiesIter->second);
        }
    }
}

}
}

