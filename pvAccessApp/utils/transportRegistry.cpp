/*
 * transportRegistry.cpp
 */

#include "transportRegistry.h"

namespace epics { namespace pvAccess {

TransportRegistry::TransportRegistry(): _mutex(Mutex())
{

}

TransportRegistry::~TransportRegistry()
{
	clear();
}

void TransportRegistry::put(Transport* transport)
{
	// TODO support type
	if(transport == NULL)
	{
		throw EpicsException("null transport provided");
	}

	Lock guard(&_mutex);
	//const string type = transport.getType();
	const int16 priority = transport->getPriority();
	const osiSockAddr* address = transport->getRemoteAddress();

	_transportsIter = _transports.find(address);
	prioritiesMap_t* priorities;
	if(_transportsIter == _transports.end())
	{
		priorities = new prioritiesMap_t();
		_transports[address] = priorities;
	}
	else
	{
		priorities = _transportsIter->second;
	}
	(*priorities)[priority] =  transport;
	_allTransports.push_back(transport);
}

Transport* TransportRegistry::get(const string type, const osiSockAddr* address, const int16 priority)
{
	// TODO support type
	if(address == NULL)
	{
		throw EpicsException("null address provided");
	}

	Lock guard(&_mutex);
	_transportsIter = _transports.find(address);
	if(_transportsIter != _transports.end())
	{
		prioritiesMap_t* priorities = _transportsIter->second;
		_prioritiesIter = priorities->find(priority);
		if(_prioritiesIter != priorities->end())
		{
			return _prioritiesIter->second;
		}
	}
	return NULL;
}

Transport** TransportRegistry::get(const string type, const osiSockAddr* address, int32& size)
{
	// TODO support type
	if(address == NULL)
	{
		throw EpicsException("null address provided");
	}

	Lock guard(&_mutex);
	_transportsIter = _transports.find(address);
	if(_transportsIter != _transports.end())
	{
		prioritiesMap_t* priorities = _transportsIter->second;
		size = priorities->size();
		Transport** transportArray = new Transport*[size];
		int i = 0;
		for(_prioritiesIter = priorities->begin(); _prioritiesIter != priorities->end(); _prioritiesIter++, i++)
		{
			transportArray[i] = _prioritiesIter->second;
		}
		return transportArray;
	}
	return NULL;
}

Transport* TransportRegistry::remove(Transport* transport)
{
	// TODO support type
	if(transport == NULL)
	{
		throw EpicsException("null transport provided");
	}

	Lock guard(&_mutex);
	const int16 priority = transport->getPriority();
	const osiSockAddr* address = transport->getRemoteAddress();
	Transport* retTransport = NULL;
	_transportsIter = _transports.find(address);
	if(_transportsIter != _transports.end())
	{
		prioritiesMap_t* priorities = _transportsIter->second;
		_prioritiesIter = priorities->find(priority);
		if(_prioritiesIter != priorities->end())
		{
			for(_allTransportsIter = _allTransports.begin(); _allTransportsIter != _allTransports.end(); _allTransportsIter++)
			{
				if(_prioritiesIter->second == *_allTransportsIter)
				{
					retTransport = _prioritiesIter->second;
					_allTransports.erase(_allTransportsIter);
					break;
				}
			}
			priorities->erase(_prioritiesIter);
			if(priorities->size() == 0)
			{
				_transports.erase(_transportsIter);
				delete priorities;
			}
		}
	}
	return retTransport;
}

void TransportRegistry::clear()
{
	Lock guard(&_mutex);
	for(_transportsIter = _transports.begin(); _transportsIter != _transports.end(); _transportsIter++)
	{
		delete _transportsIter->second;
	}

	_transports.clear();
	_allTransports.clear();
}

int TransportRegistry::numberOfActiveTransports()
{
	Lock guard(&_mutex);
	return (int32)_allTransports.size();
}

Transport** TransportRegistry::toArray(const string type, int32& size)
{
	// TODO support type
	Lock guard(&_mutex);
	size = _allTransports.size();
	Transport** transportArray = new Transport*[size];
	int i = 0;
	for(_allTransportsIter = _allTransports.begin(); _allTransportsIter != _allTransports.end(); _allTransportsIter++, i++)
	{
		transportArray[i] = *_allTransportsIter;
	}
	return transportArray;
}

Transport** TransportRegistry::toArray(int32& size)
{
	Lock guard(&_mutex);
	size = _allTransports.size();
	Transport** transportArray = new Transport*[size];
	int i = 0;
	for(_allTransportsIter = _allTransports.begin(); _allTransportsIter != _allTransports.end(); _allTransportsIter++, i++)
	{
		transportArray[i] = *_allTransportsIter;
	}
	return transportArray;
}

}}

