/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef SERVERCHANNEL_H_
#define SERVERCHANNEL_H_

#include <pv/destroyable.h>
#include <pv/remote.h>
#include <pv/clientContextImpl.h>
#include <pv/security.h>

namespace epics {
namespace pvAccess {

class ServerChannelImpl : public ServerChannel
{
public:
    POINTER_DEFINITIONS(ServerChannelImpl);

    /**
     * Create server channel for given process variable.
     * @param channel local channel.
     * @param cid channel CID.
     * @param sid channel SID.
     * @param css channel security session.
     */
    ServerChannelImpl(Channel::shared_pointer const & channel, pvAccessID cid, pvAccessID sid, ChannelSecuritySession::shared_pointer const & css);
    /*
     * Destructor.
     */
    virtual ~ServerChannelImpl();

    /**
     * Get local channel.
     * @return local channel.
     */
    Channel::shared_pointer getChannel();

    /**
     * Get channel CID.
     * @return channel CID.
     */
    pvAccessID getCID() const;

    /**
     * Get channel SID.
     * @return channel SID.
     */
    virtual pvAccessID getSID() const OVERRIDE FINAL;

    /**
     * Get ChannelSecuritySession instance.
     * @return the ChannelSecuritySession instance.
     */
    ChannelSecuritySession::shared_pointer getChannelSecuritySession() const;

    /**
     * Register request
     * @param id request ID.
     * @param request request to be registered.
     */
    void registerRequest(pvAccessID id, Destroyable::shared_pointer const & request);

    /**
     * Unregister request.
     * @param id request ID.
     */
    void unregisterRequest(pvAccessID id);

    /**
     * Get request by its ID.
     * @param id request ID.
     * @return request with given ID, <code>null</code> if there is no request with such ID.
     */
    Destroyable::shared_pointer getRequest(pvAccessID id);

    /**
     * Destroy server channel.
     */
    virtual void destroy() OVERRIDE FINAL;

    /**
     * Prints detailed information about the process variable to the standard output stream.
     */
    void printInfo();

    /**
     * Prints detailed information about the process variable to the specified output
     * stream.
     * @param fd the output stream.
     */
    void printInfo(FILE *fd);
private:
    /**
     * Local channel.
     */
    Channel::shared_pointer _channel;

    /**
     * Channel CID.
     */
    pvAccessID _cid;

    /**
     * Channel SID.
     */
    pvAccessID _sid;

    typedef std::map<pvAccessID, Destroyable::shared_pointer> _requests_t;
    /**
     * Requests.
     */
    _requests_t _requests;

    /**
     * Destroy state.
     */
    bool _destroyed;

    /**
     * Mutex
     */
    epics::pvData::Mutex _mutex;

    /**
     * Channel security session.
     */
    ChannelSecuritySession::shared_pointer _channelSecuritySession;
};

}
}


#endif /* SERVERCHANNEL_H_ */
