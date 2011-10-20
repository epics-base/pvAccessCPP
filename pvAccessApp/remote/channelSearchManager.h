/*
 * channelSearchManager.h
 */

#ifndef CHANNELSEARCHMANAGER_H
#define CHANNELSEARCHMANAGER_H

#include <pv/remote.h>
#include <pv/pvAccess.h>
#include <pv/caConstants.h>
#include <pv/blockingUDP.h>

#include <pv/timeStamp.h>
#include <osiSock.h>
#include <pv/lock.h>
#include <pv/timer.h>

#include <iostream>
#include <float.h>
#include <math.h>

#include <deque>

namespace epics { namespace pvAccess {

//TODO check the const of parameters

/**
 * SearchInstance.
 */
class SearchInstance {
public:
    
    typedef std::deque<SearchInstance*> List; 
    
	/**
	 * Destructor
	 */
	virtual ~SearchInstance() {};
	/**
	 * Return channel ID.
	 *
	 * @return channel ID.
	 */
	virtual pvAccessID getSearchInstanceID() = 0;
	/**
	 * Return search instance, e.g. channel, name.
	 *
	 * @return channel channel name.
	 */
	virtual epics::pvData::String getSearchInstanceName() = 0;
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
	virtual void addAndSetListOwnership(List* newOwner, epics::pvData::Mutex* ownerMutex, epics::pvData::int32 index) = 0;
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
	virtual epics::pvData::int32 getOwnerIndex() = 0;
	/**
	 * Generates request message.
	 */
	virtual bool generateSearchRequestMessage(epics::pvData::ByteBuffer* requestMessage, TransportSendControl* control) = 0;

	/**
	 * Search response from server (channel found).
	 * @param minorRevision	server minor CA revision.
	 * @param serverAddress	server address.
	 */
	virtual void searchResponse(epics::pvData::int8 minorRevision, osiSockAddr* serverAddress) = 0;
};

/**
 * BaseSearchInstance.
 */
class BaseSearchInstance : public SearchInstance
{
public:
	virtual ~BaseSearchInstance() {};
	void initializeSearchInstance();
	virtual pvAccessID getSearchInstanceID() = 0;
	virtual epics::pvData::String getSearchInstanceName() = 0;
	virtual void unsetListOwnership();
	virtual void addAndSetListOwnership(List* newOwner, epics::pvData::Mutex* ownerMutex, epics::pvData::int32 index);
	virtual void removeAndUnsetListOwnership();
	virtual epics::pvData::int32 getOwnerIndex();
	/**
	 * Send search message.
	 * @return success status.
	 */
	virtual bool generateSearchRequestMessage(epics::pvData::ByteBuffer* requestMessage, TransportSendControl* control);
private:
	epics::pvData::Mutex _mutex;
	List* _owner;
	epics::pvData::Mutex* _ownerMutex;
	epics::pvData::int32 _ownerIndex;

	const static int DATA_COUNT_POSITION;
	const static int PAYLOAD_POSITION;
};

class ChannelSearchManager;
/**
 * SearchTimer.
 */
class SearchTimer: public epics::pvData::TimerCallback
{
public:
    /**
	 * Constructor;
	 * @param timerIndex this timer instance index.
	 * @param allowBoost is boost allowed flag.
	 */
	SearchTimer(ChannelSearchManager* csmanager,epics::pvData::int32 timerIndex, bool allowBoost, bool allowSlowdown);
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
	void searchResponse(epics::pvData::int32 responseSequenceNumber, bool isSequenceNumberValid, epics::pvData::int64 responseTime);
	/**
	 * Calculate search time period.
	 * @return search time period.
	 */
	const epics::pvData::int64 period();
private:
	/**
	 * Instance of the channel search manager with which this search timer
	 * is associated.
	 */
	ChannelSearchManager* _chanSearchManager;
	/**
	 * Number of search attempts in one frame.
	 */
	epics::pvData::int32 _searchAttempts;
	/**
	 * Number of search responses in one frame.
	 */
	epics::pvData::int32 _searchRespones;
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
    epics::pvData::int32 _startSequenceNumber;
    /**
     * End sequence number (last frame number within a search try).
     */
    epics::pvData::int32 _endSequenceNumber;
    /**
     * This timer index.
     */
    const epics::pvData::int32 _timerIndex;
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
    // TODO replace with stl::deque<SearchInstance::shared_pointer>
    SearchInstance::List* _requestPendingChannels;
	/**
	 * Ordered (as inserted) list of channels with search request pending.
	 */
    // TODO replace with stl::deque<SearchInstance::shared_pointer>
    SearchInstance::List* _responsePendingChannels;
	/**
	 * Timer node.
	 * (sync on requestPendingChannels)
	 */
	epics::pvData::TimerNode* _timerNode;
	/**
	 * Cancel this instance.
	 */
	bool _canceled;
    /**
     * Time of last response check.
     */
    epics::pvData::int64 _timeAtResponseCheck;
    /**
     * epics::pvData::Mutex for request pending channel list.
     */
    epics::pvData::Mutex _requestPendingChannelsMutex;
    /**
     * epics::pvData::Mutex for request pending channel list.
     */
    epics::pvData::Mutex _responsePendingChannelsMutex;
    /**
     * General mutex.
     */
    epics::pvData::Mutex _mutex;
    /**
     * Volatile varialbe mutex.
     */
    epics::pvData::Mutex _volMutex;
	/**
	 * Max search tries per frame.
	 */
	static const epics::pvData::int32 MAX_FRAMES_PER_TRY;
};

class MockTransportSendControl: public TransportSendControl
{
public:
	void endMessage() {}
	void flush(bool lastMessageCompleted) {}
	void setRecipient(const osiSockAddr& sendTo) {}
	void startMessage(epics::pvData::int8 command, int ensureCapacity) {}
	void ensureBuffer(int size) {}
	void alignBuffer(int alignment) {}
	void flushSerializeBuffer() {}
};


class ChannelSearchManager
{
public:
    typedef std::tr1::shared_ptr<ChannelSearchManager> shared_pointer;
    typedef std::tr1::shared_ptr<const ChannelSearchManager> const_shared_pointer;
    
    /**
	 * Constructor.
	 * @param context
	 */
	ChannelSearchManager(Context* context);
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
	epics::pvData::int32 registeredChannelCount();
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
	void searchResponse(epics::pvData::int32 cid, epics::pvData::int32 seqNo, epics::pvData::int8 minorRevision, osiSockAddr* serverAddress);
	/**
	 * Beacon anomaly detected.
	 * Boost searching of all channels.
	 */
	void beaconAnomalyNotify();
private:
	/**
	 * Minimal RTT (ms).
	 */
	static const epics::pvData::int64 MIN_RTT;
	/**
	 * Maximal RTT (ms).
	 */
	static const epics::pvData::int64 MAX_RTT;
	/**
	 * Rate to be considered as OK.
	 */
	static const double SUCCESS_RATE;
	/**
	 * Context.
	 */
	Context* _context;
	/**
	 * Canceled flag.
	 */
	bool _canceled;
	/**
	 * Round-trip time (RTT) mean.
	 */
	double _rttmean;
	/**
	 * Search timers array.
	 * Each timer with a greater index has longer (doubled) search period.
	 */
	SearchTimer** _timers;
	/**
	 * Number of timers in timers array.
	 */
	epics::pvData::int32 _numberOfTimers;
	/**
	 * Index of a timer to be used when beacon anomaly is detected.
	 */
	epics::pvData::int32 _beaconAnomalyTimerIndex;
	/**
	 * Search (datagram) sequence number.
	 */
	epics::pvData::int32 _sequenceNumber;
	/**
	 * Max search period (in ms).
	 */
	static const epics::pvData::int64 MAX_SEARCH_PERIOD;
	/**
	 * Max search period (in ms) - lower limit.
	 */
	static const epics::pvData::int64 MAX_SEARCH_PERIOD_LOWER_LIMIT;
	/**
	 * Beacon anomaly search period (in ms).
	 */
	static const epics::pvData::int64 BEACON_ANOMALY_SEARCH_PERIOD;
	/**
	 * Max number of timers.
	 */
	static const epics::pvData::int32 MAX_TIMERS;
	/**
	 * Send byte buffer (frame)
	 */
	epics::pvData::ByteBuffer* _sendBuffer;
    /**
     * Time of last frame send.
     */
    epics::pvData::int64 _timeAtLastSend;
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
    epics::pvData::Mutex _mutex;
    /**
     * Channel mutex.
     */
    epics::pvData::Mutex _channelMutex;
    /**
     * Volatile variable mutex.
     */
    epics::pvData::Mutex _volMutex;
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
    void searchResponseTimeout(SearchInstance* channel, epics::pvData::int32 timerIndex);
	/**
	 * Boost searching of a channel.
	 * @param channel channel to boost searching.
	 * @param timerIndex to what timer-index to boost
	 */
	void boostSearching(SearchInstance* channel, epics::pvData::int32 timerIndex);
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
	epics::pvData::int32 getSequenceNumber();
	/**
	 * Get time at last send (when sendBuffer was flushed).
	 * @return time at last send.
	 */
	epics::pvData::int64 getTimeAtLastSend();
};

}}

#endif  /* CHANNELSEARCHMANAGER_H */
