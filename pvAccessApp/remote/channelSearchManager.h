/*
 * channelSearchManager.h
 */

#ifndef CHANNELSEARCHMANAGER_H
#define CHANNELSEARCHMANAGER_H

#include "remote.h"
#include "pvAccess.h"
#include "arrayFIFO.h"
#include "caConstants.h"
#include "blockingUDP.h"

#include <timeStamp.h>
#include <osiSock.h>
#include <lock.h>
#include <timer.h>

#include <iostream>
#include <float.h>
#include <math.h>

using namespace epics::pvData;

namespace epics { namespace pvAccess {

typedef int32 pvAccessID;

enum QoS {
	/**
	 * Default behavior.
	 */
	DEFAULT = 0x00,
	/**
	 * Require reply (acknowledgment for reliable operation).
	 */
	REPLY_REQUIRED = 0x01,
	/**
	 * Best-effort option (no reply).
	 */
	BESY_EFFORT = 0x02,
	/**
	 * Process option.
	 */
	PROCESS = 0x04,
	/**
	* Initialize option.
	 */
	INIT = 0x08,
	/**
	 * Destroy option.
	 */
	DESTROY = 0x10,
	/**
	 * Share data option.
	 */
	SHARE = 0x20,
	/**
	 * Get.
	 */
	GET = 0x40,
	/**
	 * Get-put.
	 */
	GET_PUT =0x80
};


//TODO this will be deleted
class ChannelImpl;
class ChannelSearchManager;
class ClientContextImpl : public ClientContext
{
private:
	Timer* _timer;
    public:

    ClientContextImpl()
    {
    	_timer = new Timer("krneki",lowPriority);
    }

    virtual Version* getVersion() {
        return NULL;
    }

    virtual ChannelProvider* getProvider() {
        return NULL;
    }

    Timer* getTimer()
	{
    	return _timer;
	}

    virtual void initialize() {

    }

    virtual void printInfo() {

    }

    virtual void printInfo(epics::pvData::StringBuilder out) {

    }

    virtual void destroy()
    {

    }

    virtual void dispose()
    {

    }

    BlockingUDPTransport* getSearchTransport()
		{
    	return NULL;
		}

	/**
	 * Searches for a channel with given channel ID.
	 * @param channelID CID.
	 * @return channel with given CID, <code>0</code> if non-existent.
	 */
	ChannelImpl* getChannel(pvAccessID channelID)
	{
		return NULL;
	}

	 ~ClientContextImpl() { delete _timer;};
    private:


    void loadConfiguration() {

    }

    void internalInitialize() {


    }

    void initializeUDPTransport() {

    }

    void internalDestroy() {

    }

    void destroyAllChannels() {

    }

	/**
	 * Check channel name.
	 */
	void checkChannelName(String& name) {

	}

	/**
	 * Check context state and tries to establish necessary state.
	 */
	void checkState() {

	}



	/**
	 * Generate Client channel ID (CID).
	 * @return Client channel ID (CID).
	 */
	pvAccessID generateCID()
	{
		return 0;
	}

	/**
	 * Free generated channel ID (CID).
	 */
	void freeCID(int cid)
	{

	}


	/**
	 * Get, or create if necessary, transport of given server address.
	 * @param serverAddress	required transport address
	 * @param priority process priority.
	 * @return transport for given address
	 */
	Transport* getTransport(TransportClient* client, osiSockAddr* serverAddress, int16 minorRevision, int16 priority)
	{

		return NULL;
	}

		/**
	 * Internal create channel.
	 */
	// TODO no minor version with the addresses
	// TODO what if there is an channel with the same name, but on different host!
	Channel* createChannelInternal(String name, ChannelRequester* requester, short priority,
			InetAddrVector* addresses) {
		return NULL;
	}

	/**
	 * Destroy channel.
	 * @param channel
	 * @param force
	 * @throws CAException
	 * @throws IllegalStateException
	 */
	void destroyChannel(ChannelImpl* channel, bool force) {


	}

	/**
	 * Get channel search manager.
	 * @return channel search manager.
	 */
	ChannelSearchManager* getChannelSearchManager() {
		return NULL;
	}
};



//TODO check the const of paramerers

/**
 * SearchInstance.
 */
class SearchInstance {
public:
	/**
	 * Destructor
	 */
	virtual ~SearchInstance() {};
	/**
	 * Return channel ID.
	 *
	 * @return channel ID.
	 */
	virtual pvAccessID getChannelID() = 0;
	/**
	 * Return channel name.
	 *
	 * @return channel channel name.
	 */
	virtual String getChannelName() = 0;
	/**
	 * Removes the owner of this search instance.
	 */
	virtual void unsetListOwnership() = 0;
	/**
	 * Adds this search instance into the provided list and set it as the owner of this search instance.
	 *
	 * @param newOwner a list to which this search instance is added.
	 * @param ownerMutex mutex belonging to the newOwner list. The mutex will be locked beofe any modification
	 * to the list will be done.
	 * @param index index of the owner (which is search timer index).
	 *
	 * @throws BaseException if the ownerMutex is NULL.
	 */
	virtual void addAndSetListOwnership(ArrayFIFO<SearchInstance*>* newOwner, Mutex* ownerMutex, int32 index) = 0;
	/**
	 * Removes this search instance from the owner list and also removes the list as the owner of this
	 * search instance.
	 *
	 * @throws BaseException if the ownerMutex is NULL.
	 */
	virtual void removeAndUnsetListOwnership() = 0;
	/**
	 * Returns the index of the owner.
	 */
	virtual int32 getOwnerIndex() = 0;
	/**
	 * Generates request message.
	 */
	virtual bool generateSearchRequestMessage(ByteBuffer* requestMessage, TransportSendControl* control) = 0;

	/**
	 * Search response from server (channel found).
	 * @param minorRevision	server minor CA revision.
	 * @param serverAddress	server address.
	 */
	virtual void searchResponse(int8 minorRevision, osiSockAddr* serverAddress) = 0;
};

/**
 * BaseSearchInstance.
 */
class BaseSearchInstance : public SearchInstance
{
public:
	virtual ~BaseSearchInstance() {};
	virtual pvAccessID getChannelID() = 0;
	virtual string getChannelName() = 0;
	virtual void unsetListOwnership();
	virtual void addAndSetListOwnership(ArrayFIFO<SearchInstance*>* newOwner, Mutex* ownerMutex, int32 index);
	virtual void removeAndUnsetListOwnership();
	virtual int32 getOwnerIndex();
	/**
	 * Send search message.
	 * @return success status.
	 */
	virtual bool generateSearchRequestMessage(ByteBuffer* requestMessage, TransportSendControl* control);
private:
	Mutex _mutex;
	ArrayFIFO<SearchInstance*>* _owner;
	Mutex* _ownerMutex;
	int32 _ownerIndex;

	const static int DATA_COUNT_POSITION;
	const static int PAYLOAD_POSITION;
};

class ChannelSearchManager;
/**
 * SearchTimer.
 */
class SearchTimer: public TimerCallback
{
public:
    /**
	 * Constructor;
	 * @param timerIndex this timer instance index.
	 * @param allowBoost is boost allowed flag.
	 */
	SearchTimer(ChannelSearchManager* csmanager,int32 timerIndex, bool allowBoost, bool allowSlowdown);
	/**
	 * Destructor.
	 */
	virtual ~SearchTimer();
	/**
	 * Shutdown this instance.
	 */
	void shutdown();
	/**
	 * Install channel.
	 * @param channel channel to be registered.
	 */
	void installChannel(SearchInstance* channel);
	/**
	 * Move channels to other <code>SearchTimer</code>.
	 * @param destination where to move channels.
	 */
	void moveChannels(SearchTimer* destination);
	/**
	 * @see TimerCallback#timerStopped()
	 */
	void timerStopped();
	/**
	 * @see TimerCallback#callback()
	 */
	void callback();
	/**
	 * Search response received notification.
	 * @param responseSequenceNumber sequence number of search frame which contained search request.
	 * @param isSequenceNumberValid valid flag of <code>responseSequenceNumber</code>.
	 * @param responseTime time of search response.
	 */
	void searchResponse(int32 responseSequenceNumber, bool isSequenceNumberValid, int64 responseTime);
	/**
	 * Calculate search time period.
	 * @return search time period.
	 */
	const int64 period();
private:
	/**
	 * Instance of the channel search manager with which this search timer
	 * is associated.
	 */
	ChannelSearchManager* _chanSearchManager;
	/**
	 * Number of search attempts in one frame.
	 */
	volatile int32 _searchAttempts;
	/**
	 * Number of search responses in one frame.
	 */
	volatile int32 _searchRespones;
	/**
	 * Number of frames per search try.
	 */
	double _framesPerTry;
	/**
	 * Number of frames until congestion threshold is reached.
	 */
	double _framesPerTryCongestThresh;
    /**
     * Start sequence number (first frame number within a search try).
     */
    volatile int32 _startSequenceNumber;
    /**
     * End sequence number (last frame number within a search try).
     */
    volatile int32 _endSequenceNumber;
    /**
     * This timer index.
     */
    const int32 _timerIndex;
    /**
     * Flag indicating whether boost is allowed.
     */
    const bool _allowBoost;
    /**
     * Flag indicating whether slow-down is allowed (for last timer).
     */
    const bool _allowSlowdown;
    /**
	 * Ordered (as inserted) list of channels with search request pending.
	 */
    ArrayFIFO<SearchInstance*>* _requestPendingChannels;
	/**
	 * Ordered (as inserted) list of channels with search request pending.
	 */
    ArrayFIFO<SearchInstance*>* _responsePendingChannels;
	/**
	 * Timer node.
	 * (sync on requestPendingChannels)
	 */
	TimerNode* _timerNode;
	/**
	 * Cancel this instance.
	 */
	volatile bool _canceled;
    /**
     * Time of last response check.
     */
    int64 _timeAtResponseCheck;
    /**
     * Mutex for request pending channel list.
     */
    Mutex _requestPendingChannelsMutex;
    /**
     * Mutex for request pending channel list.
     */
    Mutex _responsePendingChannelsMutex;
    /**
     * General mutex.
     */
    Mutex _mutex;
    /**
     * Volatile varialbe mutex.
     */
    Mutex _volMutex;
	/**
	 * Max search tries per frame.
	 */
	static const int32 MAX_FRAMES_PER_TRY;
};

class MockTransportSendControl: public TransportSendControl
{
public:
	void endMessage() {}
	void flush(bool lastMessageCompleted) {}
	void setRecipient(const osiSockAddr& sendTo) {}
	void startMessage(int8 command, int32 ensureCapacity) {}
	void ensureBuffer(int32 size) {}
	void flushSerializeBuffer() {}
};


class ChannelSearchManager
{
public:
    /**
	 * Constructor.
	 * @param context
	 */
	ChannelSearchManager(ClientContextImpl* context);
    /**
	 * Constructor.
	 * @param context
	 */
	virtual ~ChannelSearchManager();
	/**
	 * Cancel.
	 */
	void cancel();
	/**
	 * Get number of registered channels.
	 * @return number of registered channels.
	 */
	int32 registeredChannelCount();
	/**
	 * Register channel.
	 * @param channel to register.
	 */
	void registerChannel(SearchInstance* channel);
	/**
	 * Unregister channel.
	 * @param channel to unregister.
	 */
	void unregisterChannel(SearchInstance* channel);
	/**
	 * Search response from server (channel found).
	 * @param cid	client channel ID.
	 * @param seqNo	search sequence number.
	 * @param minorRevision	server minor CA revision.
	 * @param serverAddress	server address.
	 */
	void searchResponse(int32 cid, int32 seqNo, int8 minorRevision, osiSockAddr* serverAddress);
	/**
	 * Beacon anomaly detected.
	 * Boost searching of all channels.
	 */
	void beaconAnomalyNotify();
private:
	/**
	 * Minimal RTT (ms).
	 */
	static const int64 MIN_RTT;
	/**
	 * Maximal RTT (ms).
	 */
	static const int64 MAX_RTT;
	/**
	 * Rate to be considered as OK.
	 */
	static const double SUCCESS_RATE;
	/**
	 * Context.
	 */
	ClientContextImpl* _context;
	/**
	 * Canceled flag.
	 */
	volatile bool _canceled;
	/**
	 * Round-trip time (RTT) mean.
	 */
	volatile double _rttmean;
	/**
	 * Search timers array.
	 * Each timer with a greater index has longer (doubled) search period.
	 */
	SearchTimer** _timers;
	/**
	 * Number of timers in timers array.
	 */
	int32 _numberOfTimers;
	/**
	 * Index of a timer to be used when beacon anomaly is detected.
	 */
	int32 _beaconAnomalyTimerIndex;
	/**
	 * Search (datagram) sequence number.
	 */
	volatile int32 _sequenceNumber;
	/**
	 * Max search period (in ms).
	 */
	static const int64 MAX_SEARCH_PERIOD;
	/**
	 * Max search period (in ms) - lower limit.
	 */
	static const int64 MAX_SEARCH_PERIOD_LOWER_LIMIT;
	/**
	 * Beacon anomaly search period (in ms).
	 */
	static const int64 BEACON_ANOMALY_SEARCH_PERIOD;
	/**
	 * Max number of timers.
	 */
	static const int32 MAX_TIMERS;
	/**
	 * Send byte buffer (frame)
	 */
	ByteBuffer* _sendBuffer;
    /**
     * Time of last frame send.
     */
    volatile int64 _timeAtLastSend;
    /**
     * Set of registered channels.
     */
    std::map<pvAccessID,SearchInstance*> _channels;
    /**
     * Iterator for the set of registered channels.
     */
    std::map<pvAccessID,SearchInstance*>::iterator _channelsIter;
    /**
     * General mutex.
     */
    Mutex _mutex;
    /**
     * Channel mutex.
     */
    Mutex _channelMutex;
    /**
     * Volatile variable mutex.
     */
    Mutex _volMutex;
    /**
     * Mock transport send control
     */
    MockTransportSendControl* _mockTransportSendControl;
    /**
     * SearchTimer is a friend.
     */
    friend class SearchTimer;
	/**
	 * Initialize send buffer.
	 */
    void initializeSendBuffer();
	/**
	 * Flush send buffer.
	 */
    void flushSendBuffer();
	/**
	 * Generate (put on send buffer) search request
	 * @param channel
	 * @param allowNewFrame flag indicating if new search request message is allowed to be put in new frame.
	 * @return <code>true</code> if new frame was sent.
	 */
    bool generateSearchRequestMessage(SearchInstance* channel, bool allowNewFrame);
    /**
     * Notify about search failure (response timeout).
     * @param channel channel whose search failed.
     * @param timerIndex index of timer which tries to search.
     */
    void searchResponseTimeout(SearchInstance* channel, int32 timerIndex);
	/**
	 * Boost searching of a channel.
	 * @param channel channel to boost searching.
	 * @param timerIndex to what timer-index to boost
	 */
	void boostSearching(SearchInstance* channel, int32 timerIndex);
	/**
	 * Update (recalculate) round-trip estimate.
	 * @param rtt new sample of round-trip value.
	 */
	void updateRTTE(long rtt);
	/**
	 * Get round-trip estimate (in ms).
	 * @return round-trip estimate (in ms).
	 */
	double getRTTE();
	/**
	 * Get search (UDP) frame sequence number.
	 * @return search (UDP) frame sequence number.
	 */
	int32 getSequenceNumber();
	/**
	 * Get time at last send (when sendBuffer was flushed).
	 * @return time at last send.
	 */
	int64 getTimeAtLastSend();
};

}}

#endif  /* CHANNELSEARCHMANAGER_H */
