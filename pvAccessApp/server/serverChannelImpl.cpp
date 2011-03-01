/*
 * serverChannelImpl.cpp
 */

#include "serverChannelImpl.h"


namespace epics { namespace pvAccess {

ServerChannelImpl::ServerChannelImpl(Channel* channel, pvAccessID cid,
			pvAccessID sid, epics::pvData::PVField* securityToken):
			_channel(channel),
			_cid(cid),
			_sid(cid),
			_destroyed(false)
{
	if (channel == NULL)
	{
		THROW_BASE_EXCEPTION("non null local channel required");
	}
}

Channel* ServerChannelImpl::getChannel()
{
	return _channel;
}

pvAccessID ServerChannelImpl::getCID()
{
	return _cid;
}

pvAccessID ServerChannelImpl::getSID()
{
	return _sid;
}

int16 ServerChannelImpl::getAccessRights()
{
	//TODO implement
	return 0;
}

void ServerChannelImpl::registerRequest(const pvAccessID id, Destroyable* request)
{
	if (request == NULL)
	{
		THROW_BASE_EXCEPTION("request == null");
	}

	Lock guard(_mutex);
	_requests[id] = request;
}

void ServerChannelImpl::unregisterRequest(const pvAccessID id)
{
	Lock guard(_mutex);
	_iter = _requests.find(id);
	if(_iter != _requests.end())
	{
		_requests.erase(_iter);
	}
}

Destroyable* ServerChannelImpl::getRequest(const pvAccessID id)
{
	_iter = _requests.find(id);
	if(_iter != _requests.end())
	{
		return _iter->second;
	}
	return NULL;
}

void ServerChannelImpl::destroy()
{
	Lock guard(_mutex);
	if (_destroyed) return;

	_destroyed = true;

	// destroy all requests
	destroyAllRequests();

	// TODO make impl that does shares channels (and does ref counting)!!!
	// try catch?
	static_cast<ChannelImpl*>(_channel)->destroy();
}

void ServerChannelImpl::printInfo()
{
	printInfo(stdout);
}

void ServerChannelImpl::printInfo(FILE *fd)
{
	fprintf(fd,"CLASS        : %s\n", typeid(*this).name());
	fprintf(fd,"CHANNEL      : %s\n", typeid(*_channel).name());
}

void ServerChannelImpl::destroyAllRequests()
{
	Lock guard(_mutex);

	// resource allocation optimization
	if (_requests.size() == 0)
		return;

	while(_requests.size() != 0)
	{
		_iter = _requests.begin();
		_iter->second->destroy();
	}

	_requests.clear();
}

}
}
