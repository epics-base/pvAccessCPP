/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef SERVERCHANNEL_H_
#define SERVERCHANNEL_H_

#include <pv/destroyable.h>
#include <pv/remote.h>
#include <pv/security.h>

namespace epics {
namespace pvAccess {

class ServerChannel
{
public:
    POINTER_DEFINITIONS(ServerChannel);

    static size_t num_instances;

    /**
     * Create server channel for given process variable.
     * @param channel local channel.
     * @param cid channel CID.
     * @param sid channel SID.
     * @param css channel security session.
     */
    ServerChannel(Channel::shared_pointer const & channel,
                      const ChannelRequester::shared_pointer& requester,
                      pvAccessID cid, pvAccessID sid,
                      ChannelSecuritySession::shared_pointer const & css);
    ~ServerChannel();

    const Channel::shared_pointer& getChannel() const { return _channel; }

    pvAccessID getCID() const { return _cid; }

    pvAccessID getSID() const { return _sid; }

    ChannelSecuritySession::shared_pointer getChannelSecuritySession() const
    { return _channelSecuritySession; }

    void registerRequest(pvAccessID id, Destroyable::shared_pointer const & request);

    void unregisterRequest(pvAccessID id);

    //! may return NULL
    Destroyable::shared_pointer getRequest(pvAccessID id);

    void destroy();

    void printInfo() const;

    void printInfo(FILE *fd) const;
private:
    /**
     * Local channel.
     */
    const Channel::shared_pointer _channel;

    const ChannelRequester::shared_pointer _requester;

    const pvAccessID _cid, _sid;

    typedef std::map<pvAccessID, Destroyable::shared_pointer> _requests_t;
    _requests_t _requests;

    bool _destroyed;

    mutable epics::pvData::Mutex _mutex;

    const ChannelSecuritySession::shared_pointer _channelSecuritySession;
};

}
}


#endif /* SERVERCHANNEL_H_ */
