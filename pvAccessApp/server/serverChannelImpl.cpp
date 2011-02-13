/*
 * serverChannelImpl.cpp
 */

#include "serverChannelImpl.h"


namespace epics { namespace pvAccess {

ServerChannelImpl::ServerChannelImpl(Channel* channel, int32 cid,
			int32 sid, epics::pvData::PVField* securityToken):
			_channel(channel),
			_cid(cid),
			_sid(cid)
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

int32 ServerChannelImpl::getCID()
{
	return _cid;
}

int32 ServerChannelImpl::getSID()
{
	return _sid;
}

int16 ServerChannelImpl::getAccessRights()
{
	//TODO implement
	return 0;
}

void ServerChannelImpl::registerRequest(const int32 id, Destroyable* const request)
{
	if (request == NULL)
	{
		THROW_BASE_EXCEPTION("request == null");
	}

	Lock guard(_mutex);
	_requests[id] = request;
}

void ServerChannelImpl::unregisterRequest(const int32 id)
{
	Lock guard(_mutex);
	std::map<int32, epics::pvData::Destroyable*>::iterator iter = _requests.find(id);
	if(iter != _requests.end())
	{
		_requests.erase(iter);
	}
}

Destroyable* ServerChannelImpl::getRequest(const int32 id)
{
	std::map<int32, epics::pvData::Destroyable*>::iterator iter = _requests.find(id);
	if(iter != _requests.end())
	{
		return iter->second;
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

	std::map<int32, epics::pvData::Destroyable*>::iterator iter = _requests.begin();
	for(; iter != _requests.end(); iter++)
	{
		iter->second->destroy();
	}
	_requests.clear();
}

}
}
