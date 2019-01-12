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
#include <pv/baseChannelRequester.h>

namespace epics {
namespace pvAccess {

class BaseChannelRequester;

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
                      pvAccessID cid, pvAccessID sid);
    ~ServerChannel();

    const Channel::shared_pointer& getChannel() const { return _channel; }

    pvAccessID getCID() const { return _cid; }

    pvAccessID getSID() const { return _sid; }

    void registerRequest(pvAccessID id, const std::tr1::shared_ptr<BaseChannelRequester>& request);

    void unregisterRequest(pvAccessID id);

    void installGetField(const GetFieldRequester::shared_pointer& gf);
    void completeGetField(GetFieldRequester *req);

    //! may return NULL
    std::tr1::shared_ptr<BaseChannelRequester> getRequest(pvAccessID id);

    void destroy();

    void printInfo() const;

    void printInfo(FILE *fd) const;
private:
    const Channel::shared_pointer _channel;

    const ChannelRequester::shared_pointer _requester;

    const pvAccessID _cid, _sid;

    //! keep alive in-progress GetField()
    GetFieldRequester::shared_pointer _active_requester;

    typedef std::map<pvAccessID, std::tr1::shared_ptr<BaseChannelRequester> > _requests_t;
    _requests_t _requests;

    bool _destroyed;

    mutable epics::pvData::Mutex _mutex;
};

}
}


#endif /* SERVERCHANNEL_H_ */
