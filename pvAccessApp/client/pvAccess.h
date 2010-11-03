/* pvAccess.h */
#ifndef PVACCESS_H
#define PVACCESS_H
#include <pvData.h>

using namespace epics::pvData;

namespace epics { namespace pvAccess { 
    
    enum AccessRights {
        /**
         * Neither read or write access is allowed.
         */
        none,
        /**
         * Read access is allowed but write access is not allowed.
         */
        read,
        /**
         * Both read and write access are allowed.
         */
        readWrite
    };
    
    	
	/**
	 * Channel connection status.
	 */
	enum ConnectionState {
		NEVER_CONNECTED, CONNECTED, DISCONNECTED, DESTROYED
	};
    	

	/**
	 * Status type enum.
	 */
	enum StatusType { 
		/** Operation completed successfully. */
		OK, 
		/** Operation completed successfully, but there is a warning message. */
		WARNING, 
		/** Operation failed due to an error. */
		ERROR, 
		/** Operation failed due to an unexpected error. */
		FATAL
	};

/**
 * Status interface.
 * @author mse
 */
class Status : public Serializable {
	public:

	/**
	 * Get status type. 
	 * @return status type, non-<code>null</code>.
	 */
	virtual StatusType getType() = 0;
	
	/**
	 * Get error message describing an error. Required if error status.
	 * @return error message.
	 */
	virtual String getMessage() = 0;
	
	/**
	 * Get stack dump where error (exception) happened. Optional. 
	 * @return stack dump.
	 */
	virtual String getStackDump() = 0;
	
	/**
	 * Convenient OK test. Same as <code>(getType() == StatusType.OK)</code>. 
	 * NOTE: this will return <code>false</code> on WARNING message although operation succeeded.
	 * To check if operation succeeded, use <code>isSuccess</code>.
	 * @return OK status.
	 * @see #isSuccess()
	 */
	virtual bool isOK() = 0;

	/**
	 * Check if operation succeeded.
	 * @return operation success status.
	 */
	virtual bool isSuccess() = 0;
};
    	
    class Channel;
    class ChannelProvider;


    /**
     * Base interface for all channel requests.
     * @author mse
     */
class ChannelRequest /* : public Destroyable */ {
    };

    /**
     * The requester for a ChannelArray.
     * @author mrk
     *
     */
class ChannelArrayRequester : public Requester {
    public:
        /**
         * The client and server have both completed the createChannelArray request.
         * @param status Completion status.
         * @param channelArray The channelArray interface or null if the request failed.
         * @param pvArray The PVArray that holds the data.
         */
        virtual void channelArrayConnect(Status *status,ChannelArray *channelArray,PVArray *pvArray) = 0;
        /**
         * The request is done. This is always called with no locks held.
         * @param status Completion status.
         */
        virtual void putArrayDone(Status status) = 0;
        /**
         * The request is done. This is always called with no locks held.
         * @param status Completion status.
         */
        virtual void getArrayDone(Status status) = 0;
        /**
         * The request is done. This is always called with no locks held.
         * @param status Completion status.
         */
        virtual void setLengthDone(Status status) = 0;
    };
    
    /**
     * Request to put and get Array Data.
     * The data is either taken from or put in the PVArray returned by ChannelArrayRequester.channelArrayConnect.
     * @author mrk
     *
     */
class ChannelArray : public ChannelRequest{
    public:
        /**
         * put to the remote array.
         * @param lastRequest Is this the last request.
         * @param offset The offset in the remote array, i.e. the PVArray returned by ChannelArrayRequester.channelArrayConnect.
         * @param count The number of elements to put.
         */
        virtual void putArray(bool lastRequest, int offset, int count);
        /**
         * get from the remote array.
         * @param lastRequest Is this the last request.
         * @param offset The offset in the remote array, i.e. the PVArray returned by ChannelArrayRequester.channelArrayConnect.
         * @param count The number of elements to get.
         */
        void getArray(bool lastRequest, int offset, int count);
        /**
         * Set the length and/or the capacity.
         * @param lastRequest Is this the last request.
         * @param length The new length. -1 means do not change.
         * @param capacity The new capacity. -1 means do not change.
         */
        void setLength(bool lastRequest, int length, int capacity);
    };
    
    
/**
 * @author mrk
 *
 */
class ChannelFindRequester {
    public:
    /**
     * @param status Completion status.
     */
    void channelFindResult(Status *status,ChannelFind *channelFind,bool wasFound) = 0;
};

/**
 * @author mrk
 *
 */
class ChannelFind {
    public:
     ChannelProvider* getChannelProvider() = 0;
     void cancelChannelFind() = 0;
};

   
   
/**
 * Requester for channelGet.
 * @author mrk
 *
 */
class ChannelGetRequester : public Requester {
    public:
    /**
     * The client and server have both completed the createChannelGet request.
     * @param status Completion status.
     * @param channelGet The channelGet interface or null if the request failed.
     * @param pvStructure The PVStructure that holds the data.
     * @param bitSet The bitSet for that shows what data has changed.
     */
    void channelGetConnect(Status *status,ChannelGet *channelGet,PVStructure *pvStructure,BitSet *bitSet) = 0;
    /**
     * The request is done. This is always called with no locks held.
     * @param status Completion status.
     */
    void getDone(Status *status) = 0;
};


/**
 * Request to get data from a channel.
 * @author mrk
 *
 */
class ChannelGet : public ChannelRequest {
    public:
    /**
     * Get data from the channel.
     * This fails if the request can not be satisfied.
     * If it fails ChannelGetRequester.getDone is called before get returns.
     * @param lastRequest Is this the last request?
     */
    void get(bool lastRequest) = 0;
};





/**
 * Requester for channelProcess.
 * @author mrk
 *
 */
class ChannelProcessRequester : public Requester {
    public:
    /**
     * The client and server have both completed the createChannelProcess request.
     * @param status Completion status.
     * @param channelProcess The channelProcess interface or null if the client could not become
     * the record processor.
     */
    void channelProcessConnect(Status *status,ChannelProcess *channelProcess) = 0;
    /**
     * The process request is done. This is always called with no locks held.
     * @param status Completion status.
     */
    void processDone(Status *status) = 0;
};


/**
 * ChannelProcess - request that a channel be processed..
 * @author mrk
 *
 */
class ChannelProcess : public ChannelRequest {
    public:
    /**
     * Issue a process request.
     * This fails if the request can not be satisfied.
     * If it fails the channelProcessRequester.processDone is called before process returns.
     * @param lastRequest Is this the last request?
     */
    void process(bool lastRequest) = 0;
};





/**
 * Requester for ChannelPut.
 * @author mrk
 *
 */
class ChannelPutRequester : public Requester {
    public:
    /**
     * The client and server have both processed the createChannelPut request.
     * @param status Completion status.
     * @param channelPut The channelPut interface or null if the request failed.
     * @param pvStructure The PVStructure that holds the data.
     * @param bitSet The bitSet for that shows what data has changed.
     */
    void channelPutConnect(Status *status,ChannelPut *channelPut,PVStructure *pvStructure,BitSet *bitSet) = 0;
    /**
     * The request is done. This is always called with no locks held.
     * @param status Completion status.
     */
    void putDone(Status *status) = 0;
    /**
     * The get request is done. This is always called with no locks held.
     * @param status Completion status.
     */
    void getDone(Status *status) = 0;
};


/**
 * Interface for a channel access put request.
 * @author mrk
 *
 */
class ChannelPut  : public ChannelRequest {
    public:
    /**
     * Put data to a channel.
     * This fails if the request can not be satisfied.
     * If it fails ChannelPutRequester.putDone is called before put returns.
     * @param lastRequest Is this the last request?
     */
    void put(bool lastRequest) = 0;
    /**
     * Get the current data.
     */
    void get() = 0;
};






/**
 * Requester for ChannelPutGet.
 * @author mrk
 *
 */
class ChannelPutGetRequester : public Requester
{
    public:
    /**
     * The client and server have both completed the createChannelPutGet request.
     * @param status Completion status.
     * @param channelPutGet The channelPutGet interface or null if the request failed.
     * @param pvPutStructure The PVStructure that holds the putData.
     * @param pvGetStructure The PVStructure that holds the getData.
     */
    void channelPutGetConnect(Status *status,ChannelPutGet *channelPutGet,
            PVStructure *pvPutStructure,PVStructure *pvGetStructure) = 0;
    /**
     * The putGet request is done. This is always called with no locks held.
     * @param status Completion status.
     */
    void putGetDone(Status *status) = 0;
    /**
     * The getPut request is done. This is always called with no locks held.
     * @param status Completion status.
     */
    void getPutDone(Status *status) = 0;
    /**
     * The getGet request is done. This is always called with no locks held.
     * @param status Completion status.
     */
    void getGetDone(Status *status) = 0;
};


/**
 * Channel access put/get request.
 * The put is performed first, followed optionally by a process request, and then by a get request.
 * @author mrk
 *
 */
class ChannelPutGet : public ChannelRequest {
    public:
    /**
     * Issue a put/get request. If process was requested when the ChannelPutGet was created this is a put, process, get.
     * This fails if the request can not be satisfied.
     * If it fails ChannelPutGetRequester.putDone is called before putGet returns.
     * @param lastRequest Is this the last request?
     */
    void putGet(bool lastRequest) = 0;
    /**
     * Get the put PVStructure. The record will not be processed.
     */
    void getPut() = 0;
    /**
     * Get the get PVStructure. The record will not be processed.
     */
    void getGet() = 0;
};





/**
 * Requester for channelGet.
 * @author mrk
 *
 */
class ChannelRPCRequester : public Requester {
    public:
    /**
     * The client and server have both completed the createChannelGet request.
     * @param status Completion status.
     * @param channelRPC The channelRPC interface or null if the request failed.
     * @param pvArgument The argument structure for an RPC request.
     * @param bitSet The bitSet for argument changes.
     */
    void channelRPCConnect(Status *status,ChannelRPC *channelRPC,PVStructure *pvArgument,BitSet *bitSet) = 0;
    /**
     * The request is done. This is always called with no locks held.
     * @param status Completion status.
     * @param pvResponse The response data for the RPC request.
     */
    void requestDone(Status *status,PVStructure *pvResponse) = 0;
};

/**
 * Requester for channelGet.
 * @author mrk
 *
 */
class ChannelRPC : public ChannelRequest {
    public:
    /**
     * Issue an RPC request to the channel.
     * This fails if the request can not be satisfied.
     * @param lastRequest Is this the last request?
     */
    void request(bool lastRequest) = 0;
};


/**
 * Requester for a getStructure request.
 * @author mrk
 *
 */
class GetFieldRequester : public Requester {
    public:
    /**
     * The client and server have both completed the getStructure request.
     * @param status Completion status.
     * @param field The Structure for the request.
     */
    void getDone(Status *status,Field *field) = 0;
};



    /**
     * Interface for accessing a channel.
     * A channel is created via a call to ChannelAccess.createChannel(String channelName).
     * @author mrk
     * @author msekoranja
     */
    class Channel : public Requester {
        public:

    	/**
         * Get the the channel provider of this channel.
         * @return The channel provider.
         */
        virtual ChannelProvider* getProvider() = 0;
    	/**
    	 * Returns the channel's remote address, e.g. "/192.168.1.101:5064" or "#C0 S1".
    	 * @return the channel's remote address.
    	 **/
    	virtual String getRemoteAddress() = 0;
    	/**
    	 * Returns the connection state of this channel.
    	 * @return the <code>ConnectionState</code> value.
    	 **/
    	virtual ConnectionState getConnectionState() = 0;
        /**
         * Destroy the channel. It will not honor any further requests.
         */
        virtual void destroy() = 0;
        /**
         * Get the channel name.
         * @return The name.
         */
        virtual String getChannelName() = 0;
        /**
         * Get the channel requester.
         * @return The requester.
         */
        virtual ChannelRequester* getChannelRequester() = 0;
        /**
         * Is the channel connected?
         * @return (false,true) means (not, is) connected.
         */
        virtual bool isConnected() = 0;
        /**
         * Get a Field which describes the subField.
         * GetFieldRequester.getDone is called after both client and server have processed the getField request.
         * This is for clients that want to introspect a PVRecord via channel access.
         * @param requester The requester.
         * @param subField The name of the subField.
         * If this is null or an empty string the returned Field is for the entire record.
         */
        virtual void getField(GetFieldRequester *requester,String subField) = 0;
        /**
         * Get the access rights for a field of a PVStructure created via a call to createPVStructure.
         * MATEJ Channel access can store this info via auxInfo.
         * @param pvField The field for which access rights is desired.
         * @return The access rights.
         */
        virtual AccessRights getAccessRights(PVField *pvField) = 0;
        /**
         * Create a ChannelProcess.
         * ChannelProcessRequester.channelProcessReady is called after both client and server are ready for
         * the client to make a process request.
         * @param channelProcessRequester The interface for notifying when this request is complete
         * and when channel completes processing.
         * @param pvRequest Additional options (e.g. triggering).
         * @return <code>ChannelProcess</code> instance.
         */
        virtual ChannelProcess* createChannelProcess(
                ChannelProcessRequester *channelProcessRequester,
        		PVStructure *pvRequest) = 0;
        /**
         * Create a ChannelGet.
         * ChannelGetRequester.channelGetReady is called after both client and server are ready for
         * the client to make a get request.
         * @param channelGetRequester The interface for notifying when this request is complete
         * and when a channel get completes.
         * @param pvRequest A structure describing the desired set of fields from the remote PVRecord.
         * This has the same form as a pvRequest to PVCopyFactory.create.
         * @return <code>ChannelGet</code> instance.
         */
        virtual ChannelGet* createChannelGet(
                ChannelGetRequester *channelGetRequester,
                PVStructure *pvRequest) = 0;
        /**
         * Create a ChannelPut.
         * ChannelPutRequester.channelPutReady is called after both client and server are ready for
         * the client to make a put request.
         * @param channelPutRequester The interface for notifying when this request is complete
         * and when a channel get completes.
         * @param pvRequest A structure describing the desired set of fields from the remote PVRecord.
         * This has the same form as a pvRequest to PVCopyFactory.create.
         * @return <code>ChannelPut</code> instance.
         */
        virtual ChannelPut* createChannelPut(
            ChannelPutRequester *channelPutRequester,
            PVStructure *pvRequest) = 0;
        /**
         * Create a ChannelPutGet.
         * ChannelPutGetRequester.channelPutGetReady is called after both client and server are ready for
         * the client to make a putGet request.
         * @param channelPutGetRequester The interface for notifying when this request is complete
         * and when a channel get completes.
         * @param pvRequest A structure describing the desired set of fields from the remote PVRecord.
         * This has the same form as a pvRequest to PVCopyFactory.create.
         * @return <code>ChannelPutGet</code> instance.
         */
        virtual ChannelPutGet* createChannelPutGet(
            ChannelPutGetRequester *channelPutGetRequester,
            PVStructure *pvRequest) = 0;
        /**
         * Create a ChannelRPC (Remote Procedure Call).
         * @param channelRPCRequester The requester.
         * @param pvRequest Request options.
         * @return <code>ChannelRPC</code> instance.
         */
        virtual ChannelRPC* createChannelRPC(ChannelRPCRequester *channelRPCRequester,PVStructure *pvRequest) = 0;
        /**
         * Create a Monitor.
         * @param monitorRequester The requester.
         * @param pvRequest A structure describing the desired set of fields from the remote PVRecord.
         * This has the same form as a pvRequest to PVCopyFactory.create.
         * @return <code>Monitor</code> instance.
         */
        virtual Monitor* createMonitor(
            MonitorRequester *monitorRequester,
            PVStructure *pvRequest) = 0;
        
        /**
         * Create a ChannelArray.
         * @param channelArrayRequester The ChannelArrayRequester
         * @param pvRequest Additional options (e.g. triggering).
         * @return <code>ChannelArray</code> instance.
         */
        virtual ChannelArray* createChannelArray(
            ChannelArrayRequester *channelArrayRequester,
            PVStructure *pvRequest) = 0;
    };
 
/**
 * Listener for connect state changes.
 * @author mrk
 *
 */
class ChannelRequester : public Requester {
    public:
    /**
     * A channel has been created. This may be called multiple times if there are multiple providers.
     * @param status Completion status.
     * @param channel The channel.
     */
    virtual void channelCreated(Status* status, Channel *channel) = 0;
    /**
     * A channel connection state change has occurred.
     * @param c The channel.
     * @param connectionState The new connection state.
     */
    virtual void channelStateChange(Channel *c, ConnectionState connectionState) = 0;
};





    /**
     * Interface for locating channel providers.
     * @author mrk
     *
     */
class ChannelAccess {
    public:
        /**
         * Get the provider with the specified name.
         * @param providerName The name of the provider.
         * @return The interface for the provider or null if the provider is not known.
         */
        virtual ChannelProvider* getProvider(String providerName) = 0;
        /**
         * Get a array of the names of all the known providers.
         * @return The names.
         */
        virtual String[] getProviderNames() = 0;
    };

    extern ChannelAccess * getChannelAccess();
    
/**
 * Interface implemented by code that can provide access to the record
 * to which a channel connects.
 * @author mrk
 *
 */
class ChannelProvider {
    public:
	
	/** Minimal priority. */
	static const short PRIORITY_MIN = 0;
	/** Maximal priority. */
	static const short PRIORITY_MAX = 99;
	/** Default priority. */
	static const short PRIORITY_DEFAULT = PRIORITY_MIN;
	/** DB links priority. */
	static const short PRIORITY_LINKS_DB = PRIORITY_MAX;
	/** Archive priority. */
	static const short PRIORITY_ARCHIVE = (PRIORITY_MAX + PRIORITY_MIN) / 2;
	/** OPI priority. */
	static const short PRIORITY_OPI = PRIORITY_MIN;

	/**
     * Terminate.
     */
    virtual void destroy() = 0;
    /**
     * Get the provider name.
     * @return The name.
     */
    virtual String getProviderName() = 0;
    /**
     * Find a channel.
     * @param channelName The channel name.
     * @param channelFindRequester The requester.
     * @return An interface for the find.
     */
    virtual ChannelFind* channelFind(String channelName,ChannelFindRequester *channelFindRequester) = 0;
    /**
     * Create a channel.
     * @param channelName The name of the channel.
     * @param channelRequester The requester.
     * @param priority channel priority, must be <code>PRIORITY_MIN</code> <= priority <= <code>PRIORITY_MAX</code>.
     * @return <code>Channel</code> instance. If channel does not exist <code>null</code> is returned and <code>channelRequester</code> notified.
     */
    virtual Channel* createChannel(String channelName,ChannelRequester *channelRequester,short priority) = 0;
    /**
     * Create a channel.
     * @param channelName The name of the channel.
     * @param channelRequester The requester.
     * @param priority channel priority, must be <code>PRIORITY_MIN</code> <= priority <= <code>PRIORITY_MAX</code>.
     * @param address address (or list of addresses) where to look for a channel. Implementation independed string.
     * @return <code>Channel</code> instance. If channel does not exist <code>null</code> is returned and <code>channelRequester</code> notified.
     */
    virtual Channel* createChannel(String channelName,ChannelRequester *channelRequester,short priority,String address) = 0;
};


/**
 * The class representing a CA Client Context.
 * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
 * @version $Id: ClientContext.java,v 1.1 2010/05/03 14:45:40 mrkraimer Exp $
 */
class ClientContext {
    public:
  
  /**
   * Get context implementation version.
   * @return version of the context implementation.
   */
  virtual Version getVersion() = 0;

  /**
   * Initialize client context. This method is called immediately after instance construction (call of constructor).
   */
  virtual void initialize() throws CAException, IllegalStateException = 0;

  /**
   * Get channel provider implementation.
   * @return the channel provider.
   */
  virtual ChannelProvider* getProvider() = 0;

  /**
   * Prints detailed information about the context to the standard output stream.
   */
  virtual void printInfo() = 0;

  /**
   * Prints detailed information about the context to the specified output stream.
   * @param out the output stream.
   */
  virtual void printInfo(StringBuilder out) = 0;

  /**
   * Clear all resources attached to this Context
   * @throws IllegalStateException if the context has been destroyed.
   */
  virtual void destroy() throws CAException, IllegalStateException = 0;

  /**
   * Dispose (destroy) server context.
   * This calls <code>destroy()</code> and silently handles all exceptions.
   */
  virtual void dispose() = 0;
};
 
    

}}

#endif  /* PVACCESS_H */
