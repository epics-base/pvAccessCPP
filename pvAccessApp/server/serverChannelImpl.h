/*
 * serverChannel.h
 */

#ifndef SERVERCHANNEL_H_
#define SERVERCHANNEL_H_

#include "remote.h"
#include "clientContextImpl.h"

#include <destroyable.h>

namespace epics {
namespace pvAccess {

class ServerChannelImpl :  public ServerChannel
{
public:
	/**
	 * Create server channel for given process variable.
	 * @param channel local channel.
	 * @param cid channel CID.
	 * @param sid channel SID.
	 * @param securityToken security token.
	 */
	ServerChannelImpl(Channel* channel, pvAccessID cid, pvAccessID sid, epics::pvData::PVField* securityToken);
	/*
	 * Destructor.
	 */
	~ServerChannelImpl() {};

	/**
	 * Get local channel.
	 * @return local channel.
	 */
	Channel* getChannel();

	/**
	 * Get channel CID.
	 * @return channel CID.
	 */
	pvAccessID getCID();

	/**
	 * Get channel SID.
	 * @return channel SID.
	 */
	pvAccessID getSID();

	/**
	 * Get access rights (bit-mask encoded).
	 * @see AccessRights
	 * @return bit-mask encoded access rights.
	 */
	int16 getAccessRights();

	/**
	 * Register request
	 * @param id request ID.
	 * @param request request to be registered.
	 */
	void registerRequest(pvAccessID id, epics::pvData::Destroyable* request);

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
    epics::pvData::Destroyable* getRequest(pvAccessID id);

    /**
     * Destroy server channel.
     */
	void destroy();

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
	Channel* _channel;

	/**
	 * Channel CID.
	 */
	pvAccessID _cid;

	/**
	 * Channel SID.
	 */
	pvAccessID _sid;

	/**
	 * Requests.
	 */
	std::map<pvAccessID, epics::pvData::Destroyable*> _requests;

	/**
	 * Requests iterator.
	 */
	std::map<pvAccessID, epics::pvData::Destroyable*>::iterator _iter;

	/**
	 * Destroy state.
	 */
	boolean _destroyed;

	/**
	 * Mutex
	 */
	epics::pvData::Mutex _mutex;

	/**
	 * Destroy all registered requests.
	 */
	void destroyAllRequests();
};

}
}


#endif /* SERVERCHANNEL_H_ */
