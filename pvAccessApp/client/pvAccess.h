/* pvAccess.h */
#ifndef PVACCESS_H
#define PVACCESS_H
#include <pvData.h>
#include <status.h>
#include <destroyable.h>
#include <monitor.h>
#include <version.h>
#include <vector>

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
                    
        extern const char* ConnectionStateNames[];
    	

        class Channel;
        class ChannelProvider;

        /**
         * Base interface for all channel requests.
         * @author mse
         */
        class ChannelRequest : public epics::pvData::Destroyable, private epics::pvData::NoDefaultMethods {
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
            virtual void putArray(bool lastRequest, int offset, int count) = 0;

            /**
             * get from the remote array.
             * @param lastRequest Is this the last request.
             * @param offset The offset in the remote array, i.e. the PVArray returned by ChannelArrayRequester.channelArrayConnect.
             * @param count The number of elements to get.
             */
            virtual void getArray(bool lastRequest, int offset, int count) = 0;

            /**
             * Set the length and/or the capacity.
             * @param lastRequest Is this the last request.
             * @param length The new length. -1 means do not change.
             * @param capacity The new capacity. -1 means do not change.
             */
            virtual void setLength(bool lastRequest, int length, int capacity) = 0;
        };

        /**
         * The epics::pvData::Requester for a ChannelArray.
         * @author mrk
         *
         */
        class ChannelArrayRequester : public epics::pvData::Requester {
            public:

            /**
             * The client and server have both completed the createChannelArray request.
             * @param status Completion status.
             * @param channelArray The channelArray interface or null if the request failed.
             * @param pvArray The PVArray that holds the data.
             */
            virtual void channelArrayConnect(epics::pvData::Status *status,ChannelArray *channelArray,epics::pvData::PVArray *pvArray) = 0;

            /**
             * The request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void putArrayDone(epics::pvData::Status *status) = 0;

            /**
             * The request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void getArrayDone(epics::pvData::Status *status) = 0;

            /**
             * The request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void setLengthDone(epics::pvData::Status *status) = 0;
        };


        /**
         * @author mrk
         *
         */
        class ChannelFind : public epics::pvData::Destroyable, private epics::pvData::NoDefaultMethods {
            public:
            virtual ChannelProvider* getChannelProvider() = 0;
            virtual void cancelChannelFind() = 0;
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
            virtual void channelFindResult(epics::pvData::Status *status,ChannelFind *channelFind,bool wasFound) = 0;
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
            virtual void get(bool lastRequest) = 0;
        };


        /**
         * epics::pvData::Requester for channelGet.
         * @author mrk
         *
         */
        class ChannelGetRequester : public epics::pvData::Requester {
            public:

            /**
             * The client and server have both completed the createChannelGet request.
             * @param status Completion status.
             * @param channelGet The channelGet interface or null if the request failed.
             * @param pvStructure The PVStructure that holds the data.
             * @param bitSet The bitSet for that shows what data has changed.
             */
            virtual void channelGetConnect(epics::pvData::Status *status,ChannelGet *channelGet,
                    epics::pvData::PVStructure *pvStructure,epics::pvData::BitSet *bitSet) = 0;

            /**
             * The request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void getDone(epics::pvData::Status *status) = 0;
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
            virtual void process(bool lastRequest) = 0;
        };


        /**
         * epics::pvData::Requester for channelProcess.
         * @author mrk
         *
         */
        class ChannelProcessRequester : public epics::pvData::Requester {
            public:

            /**
             * The client and server have both completed the createChannelProcess request.
             * @param status Completion status.
             * @param channelProcess The channelProcess interface or null if the client could not become
             * the record processor.
             */
            virtual void channelProcessConnect(epics::pvData::Status *status,ChannelProcess *channelProcess) = 0;

            /**
             * The process request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void processDone(epics::pvData::Status *status) = 0;
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
            virtual void put(bool lastRequest) = 0;

            /**
             * Get the current data.
             */
            virtual void get() = 0;
        };


        /**
         * epics::pvData::Requester for ChannelPut.
         * @author mrk
         *
         */
        class ChannelPutRequester : public epics::pvData::Requester {
            public:

            /**
             * The client and server have both processed the createChannelPut request.
             * @param status Completion status.
             * @param channelPut The channelPut interface or null if the request failed.
             * @param pvStructure The PVStructure that holds the data.
             * @param bitSet The bitSet for that shows what data has changed.
             */
            virtual void channelPutConnect(epics::pvData::Status *status,ChannelPut *channelPut,
                    epics::pvData::PVStructure *pvStructure,epics::pvData::BitSet *bitSet) = 0;

            /**
             * The request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void putDone(epics::pvData::Status *status) = 0;

            /**
             * The get request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void getDone(epics::pvData::Status *status) = 0;
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
            virtual void putGet(bool lastRequest) = 0;

            /**
             * Get the put PVStructure. The record will not be processed.
             */
            virtual void getPut() = 0;

            /**
             * Get the get PVStructure. The record will not be processed.
             */
            virtual void getGet() = 0;
        };


        /**
         * epics::pvData::Requester for ChannelPutGet.
         * @author mrk
         *
         */
        class ChannelPutGetRequester : public epics::pvData::Requester
        {
            public:

            /**
             * The client and server have both completed the createChannelPutGet request.
             * @param status Completion status.
             * @param channelPutGet The channelPutGet interface or null if the request failed.
             * @param pvPutStructure The PVStructure that holds the putData.
             * @param pvGetStructure The PVStructure that holds the getData.
             */
            virtual void channelPutGetConnect(epics::pvData::Status *status,ChannelPutGet *channelPutGet,
                        epics::pvData::PVStructure *pvPutStructure,epics::pvData::PVStructure *pvGetStructure) = 0;
            /**
             * The putGet request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void putGetDone(epics::pvData::Status *status) = 0;

            /**
             * The getPut request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void getPutDone(epics::pvData::Status *status) = 0;

            /**
             * The getGet request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void getGetDone(epics::pvData::Status *status) = 0;
        };


        /**
         * epics::pvData::Requester for channelGet.
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
            virtual void request(bool lastRequest) = 0;
        };


        /**
         * epics::pvData::Requester for channelGet.
         * @author mrk
         *
         */
        class ChannelRPCRequester : public epics::pvData::Requester {
            public:

            /**
             * The client and server have both completed the createChannelGet request.
             * @param status Completion status.
             * @param channelRPC The channelRPC interface or null if the request failed.
             * @param pvArgument The argument structure for an RPC request.
             * @param bitSet The bitSet for argument changes.
             */
            virtual void channelRPCConnect(epics::pvData::Status *status,ChannelRPC *channelRPC,
                        epics::pvData::PVStructure *pvArgument,epics::pvData::BitSet *bitSet) = 0;

            /**
             * The request is done. This is always called with no locks held.
             * @param status Completion status.
             * @param pvResponse The response data for the RPC request.
             */
            virtual void requestDone(epics::pvData::Status *status,epics::pvData::PVStructure *pvResponse) = 0;
        };


        /**
         * epics::pvData::Requester for a getStructure request.
         * @author mrk
         *
         */
        class GetFieldRequester : public epics::pvData::Requester {
            public:

            /**
             * The client and server have both completed the getStructure request.
             * @param status Completion status.
             * @param field The Structure for the request.
             */
            virtual void getDone(epics::pvData::Status *status,epics::pvData::FieldConstPtr field) = 0;
        };


        /**
         * Listener for connect state changes.
         * @author mrk
         *
         */
        class ChannelRequester : public epics::pvData::Requester {
            public:

            /**
             * A channel has been created. This may be called multiple times if there are multiple providers.
             * @param status Completion status.
             * @param channel The channel.
             */
            virtual void channelCreated(epics::pvData::Status* status, Channel *channel) = 0;

            /**
             * A channel connection state change has occurred.
             * @param c The channel.
             * @param connectionState The new connection state.
             */
            virtual void channelStateChange(Channel *c, ConnectionState connectionState) = 0;
        };


        /**
         * Interface for accessing a channel.
         * A channel is created via a call to ChannelAccess.createChannel(String channelName).
         * @author mrk
         * @author msekoranja
         */
        class Channel : 
                public epics::pvData::Requester,
                public epics::pvData::Destroyable,
                private epics::pvData::NoDefaultMethods {
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
            virtual epics::pvData::String getRemoteAddress() = 0;

            /**
             * Returns the connection state of this channel.
             * @return the <code>ConnectionState</code> value.
             **/
            virtual ConnectionState getConnectionState() = 0;

            /**
             * Get the channel name.
             * @return The name.
             */
            virtual epics::pvData::String getChannelName() = 0;

            /**
             * Get the channel epics::pvData::Requester.
             * @return The epics::pvData::Requester.
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
             * @param epics::pvData::Requester The epics::pvData::Requester.
             * @param subField The name of the subField.
             * If this is null or an empty epics::pvData::String the returned Field is for the entire record.
             */
            virtual void getField(GetFieldRequester *requester,epics::pvData::String subField) = 0;

            /**
             * Get the access rights for a field of a PVStructure created via a call to createPVStructure.
             * MATEJ Channel access can store this info via auxInfo.
             * @param pvField The field for which access rights is desired.
             * @return The access rights.
             */
            virtual AccessRights getAccessRights(epics::pvData::PVField *pvField) = 0;

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
                    epics::pvData::PVStructure *pvRequest) = 0;

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
                    epics::pvData::PVStructure *pvRequest) = 0;

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
                    epics::pvData::PVStructure *pvRequest) = 0;

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
                    epics::pvData::PVStructure *pvRequest) = 0;

            /**
             * Create a ChannelRPC (Remote Procedure Call).
             * @param channelRPCRequester The epics::pvData::Requester.
             * @param pvRequest Request options.
             * @return <code>ChannelRPC</code> instance.
             */
            virtual ChannelRPC* createChannelRPC(ChannelRPCRequester *channelRPCRequester,
                    epics::pvData::PVStructure *pvRequest) = 0;

            /**
             * Create a Monitor.
             * @param monitorRequester The epics::pvData::Requester.
             * @param pvRequest A structure describing the desired set of fields from the remote PVRecord.
             * This has the same form as a pvRequest to PVCopyFactory.create.
             * @return <code>Monitor</code> instance.
             */
            virtual epics::pvData::Monitor* createMonitor(
                    epics::pvData::MonitorRequester *monitorRequester,
                    epics::pvData::PVStructure *pvRequest) = 0;

            /**
             * Create a ChannelArray.
             * @param channelArrayRequester The ChannelArrayRequester
             * @param pvRequest Additional options (e.g. triggering).
             * @return <code>ChannelArray</code> instance.
             */
            virtual ChannelArray* createChannelArray(
                    ChannelArrayRequester *channelArrayRequester,
                    epics::pvData::PVStructure *pvRequest) = 0;
        };



        /**
         * Interface for locating channel providers.
         * @author mrk
         *
         */
        class ChannelAccess : private epics::pvData::NoDefaultMethods {
            public:
            virtual ~ChannelAccess() {};

            /**
             * Get the provider with the specified name.
             * @param providerName The name of the provider.
             * @return The interface for the provider or null if the provider is not known.
             */
            virtual ChannelProvider* getProvider(epics::pvData::String providerName) = 0;

            /**
             * Get a array of the names of all the known providers.
             * @return The names. Be sure to delete vector instance.
             */
            virtual std::vector<epics::pvData::String>* getProviderNames() = 0;
        };

        extern ChannelAccess * getChannelAccess();
        extern void registerChannelProvider(ChannelProvider *channelProvider);
        extern void unregisterChannelProvider(ChannelProvider *channelProvider);

        /**
         * Interface implemented by code that can provide access to the record
         * to which a channel connects.
         * @author mrk
         *
         */
        class ChannelProvider : public epics::pvData::Destroyable, private epics::pvData::NoDefaultMethods {
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
             * Get the provider name.
             * @return The name.
             */
            virtual epics::pvData::String getProviderName() = 0;

            /**
             * Find a channel.
             * @param channelName The channel name.
             * @param channelFindRequester The epics::pvData::Requester.
             * @return An interface for the find.
             */
            virtual ChannelFind* channelFind(epics::pvData::String channelName,ChannelFindRequester *channelFindRequester) = 0;

            /**
             * Create a channel.
             * @param channelName The name of the channel.
             * @param channelRequester The epics::pvData::Requester.
             * @param priority channel priority, must be <code>PRIORITY_MIN</code> <= priority <= <code>PRIORITY_MAX</code>.
             * @return <code>Channel</code> instance. If channel does not exist <code>null</code> is returned and <code>channelRequester</code> notified.
             */
            virtual Channel* createChannel(epics::pvData::String channelName,ChannelRequester *channelRequester,short priority = PRIORITY_DEFAULT) = 0;

            /**
             * Create a channel.
             * @param channelName The name of the channel.
             * @param channelRequester The epics::pvData::Requester.
             * @param priority channel priority, must be <code>PRIORITY_MIN</code> <= priority <= <code>PRIORITY_MAX</code>.
             * @param address address (or list of addresses) where to look for a channel. Implementation independed epics::pvData::String.
             * @return <code>Channel</code> instance. If channel does not exist <code>null</code> is returned and <code>channelRequester</code> notified.
             */
            virtual Channel* createChannel(epics::pvData::String channelName,ChannelRequester *channelRequester,short priority,epics::pvData::String address) = 0;
        };


        /**
         * The class representing a CA Client Context.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         */
        class ClientContext : public epics::pvData::Destroyable, private epics::pvData::NoDefaultMethods {
            public:

            /**
             * Get context implementation version.
             * @return version of the context implementation.
             */
            virtual Version* getVersion() = 0;

            /**
             * Initialize client context. This method is called immediately after instance construction (call of constructor).
             */
            virtual void initialize() = 0;

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
            virtual void printInfo(epics::pvData::StringBuilder out) = 0;

            /**
             * Dispose (destroy) server context.
             * This calls <code>destroy()</code> and silently handles all exceptions.
             */
            virtual void dispose() = 0;
        };


        /**
         * Interface for creating request structure.
         * @author mse
         *
         */
        class CreateRequest : private epics::pvData::NoDefaultMethods {
            public:
            virtual ~CreateRequest() {};

            /**
             * Create a request structure for the create calls in Channel.
             * See the package overview documentation for details.
             * @param request The field request. See the package overview documentation for details.
             * @param requester The requester;
             * @return The request structure if an invalid request was given. 
             */
        	virtual epics::pvData::PVStructure* createRequest(String request, epics::pvData::Requester* requester) = 0;
        };

        extern CreateRequest * getCreateRequest();


    }}

#endif  /* PVACCESS_H */
