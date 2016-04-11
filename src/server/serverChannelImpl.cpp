/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define epicsExportSharedSymbols
#include <pv/serverChannelImpl.h>

using namespace epics::pvData;

namespace epics {
namespace pvAccess {

ServerChannelImpl::ServerChannelImpl(Channel::shared_pointer const & channel,
                                     pvAccessID cid, pvAccessID sid,
                                     ChannelSecuritySession::shared_pointer const & css):
    _channel(channel),
    _cid(cid),
    _sid(sid),
    _destroyed(false),
    _channelSecuritySession(css)
{
    if (!channel.get())
    {
        THROW_BASE_EXCEPTION("non-null channel required");
    }
}

Channel::shared_pointer ServerChannelImpl::getChannel()
{
    return _channel;
}

pvAccessID ServerChannelImpl::getCID() const
{
    return _cid;
}

pvAccessID ServerChannelImpl::getSID() const
{
    return _sid;
}

ChannelSecuritySession::shared_pointer ServerChannelImpl::getChannelSecuritySession() const
{
    return _channelSecuritySession;
}

void ServerChannelImpl::registerRequest(const pvAccessID id, Destroyable::shared_pointer const & request)
{
    Lock guard(_mutex);
    _requests[id] = request;
}

void ServerChannelImpl::unregisterRequest(const pvAccessID id)
{
    Lock guard(_mutex);
    std::map<pvAccessID, epics::pvData::Destroyable::shared_pointer>::iterator iter = _requests.find(id);
    if(iter != _requests.end())
    {
        _requests.erase(iter);
    }
}

Destroyable::shared_pointer ServerChannelImpl::getRequest(const pvAccessID id)
{
    Lock guard(_mutex);
    std::map<pvAccessID, epics::pvData::Destroyable::shared_pointer>::iterator iter = _requests.find(id);
    if(iter != _requests.end())
    {
        return iter->second;
    }
    return Destroyable::shared_pointer();
}

void ServerChannelImpl::destroy()
{
    Lock guard(_mutex);
    if (_destroyed) return;
    _destroyed = true;

    // destroy all requests
    destroyAllRequests();

    // close channel security session
    // TODO try catch
    _channelSecuritySession->close();

    // ... and the channel
    // TODO try catch
    _channel->destroy();
}

ServerChannelImpl::~ServerChannelImpl()
{
    destroy();
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
        _requests.begin()->second->destroy();
    }

    _requests.clear();
}

}
}
