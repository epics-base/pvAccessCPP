/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVACCESS_H
#define PVACCESS_H
#include <pv/pvData.h>
#include <pv/status.h>
#include <pv/destroyable.h>
#include <pv/monitor.h>
#include <pv/pvaVersion.h>
#include <vector>
#include <pv/bitSet.h>

namespace epics {
namespace pvAccess { 

        // TODO add write-only?
        // change names
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
         * 
         */
        class Lockable
        {
            public:
            POINTER_DEFINITIONS(Lockable);
            
            virtual ~Lockable() {};

            virtual void lock() = 0;
            virtual void unlock() = 0;
        };

        /**
         * Scope lock.
         */
        class ScopedLock : private epics::pvData::NoDefaultMethods {
            public:
            
            explicit ScopedLock(Lockable::shared_pointer const & li)
                : lockable(li), locked(true) {
                lockable->lock();
            }
            
            ~ScopedLock() {
                unlock();
            }
            
            void lock() {
                if(!locked) { 
                    lockable->lock();
                    locked = true;
                }
            }
            
            void unlock() {
                if(locked) {
                    lockable->unlock();
                    locked=false;
                }
            }
            
            bool ownsLock() const {
                return locked;
            }
        
            private:
            
            Lockable::shared_pointer const lockable;
            bool locked;
        };
        

        class Channel;
        class ChannelProvider;

        /**
         * Base interface for all channel requests.
         */
        class ChannelRequest : public epics::pvData::Destroyable, public Lockable, private epics::pvData::NoDefaultMethods {
            public:
            POINTER_DEFINITIONS(ChannelRequest);
        };

        /**
         * Request to put and get Array Data.
         * The data is either taken from or put in the PVArray returned by ChannelArrayRequester.channelArrayConnect.
         */
        class ChannelArray : public ChannelRequest{
            public:
            POINTER_DEFINITIONS(ChannelArray);

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
         */
        class ChannelArrayRequester : virtual public epics::pvData::Requester {
            public:
            POINTER_DEFINITIONS(ChannelArrayRequester);

            /**
             * The client and server have both completed the createChannelArray request.
             * @param status Completion status.
             * @param channelArray The channelArray interface or null if the request failed.
             * @param pvArray The PVArray that holds the data.
             */
            virtual void channelArrayConnect(
                    const epics::pvData::Status& status,
                    ChannelArray::shared_pointer const & channelArray,
                    epics::pvData::PVArray::shared_pointer const & pvArray) = 0;

            /**
             * The request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void putArrayDone(const epics::pvData::Status& status) = 0;

            /**
             * The request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void getArrayDone(const epics::pvData::Status& status) = 0;

            /**
             * The request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void setLengthDone(const epics::pvData::Status& status) = 0;
        };


        /**
         *
         */
        class ChannelFind : public epics::pvData::Destroyable, private epics::pvData::NoDefaultMethods {
            public:
            POINTER_DEFINITIONS(ChannelFind);

            virtual std::tr1::shared_ptr<ChannelProvider> getChannelProvider() = 0;
            virtual void cancelChannelFind() = 0;
        };

        /**
         *
         */
        class ChannelFindRequester {
            public:
            POINTER_DEFINITIONS(ChannelFindRequester);

            virtual ~ChannelFindRequester() {};
            /**
             * @param status Completion status.
             */
            virtual void channelFindResult(const epics::pvData::Status& status,ChannelFind::shared_pointer const & channelFind,bool wasFound) = 0;
        };


        /**
         * Request to get data from a channel.
         */
        class ChannelGet : public ChannelRequest {
            public:
            POINTER_DEFINITIONS(ChannelGet);

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
         */
        class ChannelGetRequester : virtual public epics::pvData::Requester {
            public:
            POINTER_DEFINITIONS(ChannelGetRequester);

            /**
             * The client and server have both completed the createChannelGet request.
             * @param status Completion status.
             * @param channelGet The channelGet interface or null if the request failed.
             * @param pvStructure The PVStructure that holds the data.
             * @param bitSet The bitSet for that shows what data has changed.
             */
            virtual void channelGetConnect(const epics::pvData::Status& status,ChannelGet::shared_pointer const & channelGet,
                    epics::pvData::PVStructure::shared_pointer const & pvStructure,epics::pvData::BitSet::shared_pointer const & bitSet) = 0;

            /**
             * The request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void getDone(const epics::pvData::Status& status) = 0;
        };


        /**
         * ChannelProcess - request that a channel be processed..
         */
        class ChannelProcess : public ChannelRequest {
            public:
            POINTER_DEFINITIONS(ChannelProcess);

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
         */
        class ChannelProcessRequester : virtual public epics::pvData::Requester {
            public:
            POINTER_DEFINITIONS(ChannelProcessRequester);

            /**
             * The client and server have both completed the createChannelProcess request.
             * @param status Completion status.
             * @param channelProcess The channelProcess interface or null if the client could not become
             * the record processor.
             */
            virtual void channelProcessConnect(const epics::pvData::Status& status,ChannelProcess::shared_pointer const & channelProcess) = 0;

            /**
             * The process request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void processDone(const epics::pvData::Status& status) = 0;
        };


        /**
         * Interface for a channel access put request.
         */
        class ChannelPut  : public ChannelRequest {
            public:
            POINTER_DEFINITIONS(ChannelPut);

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
         */
        class ChannelPutRequester : virtual public epics::pvData::Requester {
            public:
            POINTER_DEFINITIONS(ChannelPutRequester);

            /**
             * The client and server have both processed the createChannelPut request.
             * @param status Completion status.
             * @param channelPut The channelPut interface or null if the request failed.
             * @param pvStructure The PVStructure that holds the data.
             * @param bitSet The bitSet for that shows what data has changed.
             */
            virtual void channelPutConnect(const epics::pvData::Status& status,ChannelPut::shared_pointer const & channelPut,
                    epics::pvData::PVStructure::shared_pointer const & pvStructure,epics::pvData::BitSet::shared_pointer const & bitSet) = 0;

            /**
             * The request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void putDone(const epics::pvData::Status& status) = 0;

            /**
             * The get request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void getDone(const epics::pvData::Status& status) = 0;
        };


        /**
         * Channel access put/get request.
         * The put is performed first, followed optionally by a process request, and then by a get request.
         */
        class ChannelPutGet : public ChannelRequest {
            public:
            POINTER_DEFINITIONS(ChannelPutGet);

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
         */
        class ChannelPutGetRequester : virtual public epics::pvData::Requester
        {
            public:
            POINTER_DEFINITIONS(ChannelPutGetRequester);

            /**
             * The client and server have both completed the createChannelPutGet request.
             * @param status Completion status.
             * @param channelPutGet The channelPutGet interface or null if the request failed.
             * @param pvPutStructure The PVStructure that holds the putData.
             * @param pvGetStructure The PVStructure that holds the getData.
             */
            virtual void channelPutGetConnect(const epics::pvData::Status& status,ChannelPutGet::shared_pointer const & channelPutGet,
                        epics::pvData::PVStructure::shared_pointer const & pvPutStructure,epics::pvData::PVStructure::shared_pointer const & pvGetStructure) = 0;
            /**
             * The putGet request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void putGetDone(const epics::pvData::Status& status) = 0;

            /**
             * The getPut request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void getPutDone(const epics::pvData::Status& status) = 0;

            /**
             * The getGet request is done. This is always called with no locks held.
             * @param status Completion status.
             */
            virtual void getGetDone(const epics::pvData::Status& status) = 0;
        };


        /**
         * epics::pvData::Requester for channelGet.
         */
        class ChannelRPC : public ChannelRequest {
            public:
            POINTER_DEFINITIONS(ChannelRPC);

            /**
             * Issue an RPC request to the channel.
             * This fails if the request can not be satisfied.
             * @param pvArgument The argument structure for an RPC request.
             * @param lastRequest Is this the last request?
             */
            virtual void request(epics::pvData::PVStructure::shared_pointer const & pvArgument, bool lastRequest) = 0;
        };


        /**
         * epics::pvData::Requester for channelGet.
         */
        class ChannelRPCRequester : virtual public epics::pvData::Requester {
            public:
            POINTER_DEFINITIONS(ChannelRPCRequester);

            /**
             * The client and server have both completed the createChannelGet request.
             * @param status Completion status.
             * @param channelRPC The channelRPC interface or null if the request failed.
             */
            virtual void channelRPCConnect(const epics::pvData::Status& status,ChannelRPC::shared_pointer const & channelRPC) = 0;

            /**
             * The request is done. This is always called with no locks held.
             * @param status Completion status.
             * @param pvResponse The response data for the RPC request.
             */
            virtual void requestDone(const epics::pvData::Status& status,epics::pvData::PVStructure::shared_pointer const & pvResponse) = 0;
        };


        /**
         * epics::pvData::Requester for a getStructure request.
         */
        class GetFieldRequester : virtual public epics::pvData::Requester {
            public:
            POINTER_DEFINITIONS(GetFieldRequester);

            /**
             * The client and server have both completed the getStructure request.
             * @param status Completion status.
             * @param field The Structure for the request.
             */
             // TODO naming convention
            virtual void getDone(const epics::pvData::Status& status,epics::pvData::FieldConstPtr const & field) = 0;
        };


        class ChannelRequester;
        
        /**
         * Interface for accessing a channel.
         * A channel is created via a call to ChannelAccess.createChannel(String channelName).
         */
        class Channel : 
                public epics::pvData::Requester,
                public epics::pvData::Destroyable,
                private epics::pvData::NoDefaultMethods {
            public:
            POINTER_DEFINITIONS(Channel);

        	/**
        	 * Channel connection status.
        	 */
        	enum ConnectionState {
                    NEVER_CONNECTED, CONNECTED, DISCONNECTED, DESTROYED
                        };
                        
            static const char* ConnectionStateNames[];
            
            /**
             * Get the the channel provider of this channel.
             * @return The channel provider.
             */
            virtual std::tr1::shared_ptr<ChannelProvider> getProvider() = 0;
//            virtual ChannelProvider::shared_pointer getProvider() = 0;

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
//            virtual ChannelRequester::shared_pointer getChannelRequester() = 0;
            virtual std::tr1::shared_ptr<ChannelRequester> getChannelRequester() = 0;

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
            virtual void getField(GetFieldRequester::shared_pointer const & requester,epics::pvData::String const & subField) = 0;

            /**
             * Get the access rights for a field of a PVStructure created via a call to createPVStructure.
             * MATEJ Channel access can store this info via auxInfo.
             * @param pvField The field for which access rights is desired.
             * @return The access rights.
             */
            virtual AccessRights getAccessRights(epics::pvData::PVField::shared_pointer const & pvField) = 0;

            /**
             * Create a ChannelProcess.
             * ChannelProcessRequester.channelProcessReady is called after both client and server are ready for
             * the client to make a process request.
             * @param channelProcessRequester The interface for notifying when this request is complete
             * and when channel completes processing.
             * @param pvRequest Additional options (e.g. triggering).
             * @return <code>ChannelProcess</code> instance.
             */
            virtual ChannelProcess::shared_pointer createChannelProcess(
                    ChannelProcessRequester::shared_pointer const & channelProcessRequester,
                    epics::pvData::PVStructure::shared_pointer const & pvRequest) = 0;

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
            virtual ChannelGet::shared_pointer createChannelGet(
                    ChannelGetRequester::shared_pointer const & channelGetRequester,
                    epics::pvData::PVStructure::shared_pointer const & pvRequest) = 0;

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
            virtual ChannelPut::shared_pointer createChannelPut(
                    ChannelPutRequester::shared_pointer const & channelPutRequester,
                    epics::pvData::PVStructure::shared_pointer const & pvRequest) = 0;

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
            virtual ChannelPutGet::shared_pointer createChannelPutGet(
                    ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
                    epics::pvData::PVStructure::shared_pointer const & pvRequest) = 0;

            /**
             * Create a ChannelRPC (Remote Procedure Call).
             * @param channelRPCRequester The epics::pvData::Requester.
             * @param pvRequest Request options.
             * @return <code>ChannelRPC</code> instance.
             */
            virtual ChannelRPC::shared_pointer createChannelRPC(
                    ChannelRPCRequester::shared_pointer const & channelRPCRequester,
                    epics::pvData::PVStructure::shared_pointer const & pvRequest) = 0;

            /**
             * Create a Monitor.
             * @param monitorRequester The epics::pvData::Requester.
             * @param pvRequest A structure describing the desired set of fields from the remote PVRecord.
             * This has the same form as a pvRequest to PVCopyFactory.create.
             * @return <code>Monitor</code> instance.
             */
            virtual epics::pvData::Monitor::shared_pointer createMonitor(
                    epics::pvData::MonitorRequester::shared_pointer const & monitorRequester,
                    epics::pvData::PVStructure::shared_pointer const & pvRequest) = 0;

            /**
             * Create a ChannelArray.
             * @param channelArrayRequester The ChannelArrayRequester
             * @param pvRequest Additional options (e.g. triggering).
             * @return <code>ChannelArray</code> instance.
             */
            virtual ChannelArray::shared_pointer createChannelArray(
                    ChannelArrayRequester::shared_pointer const & channelArrayRequester,
                    epics::pvData::PVStructure::shared_pointer const & pvRequest) = 0;

            /**
             * Prints detailed information about the context to the standard output stream.
             */
            virtual void printInfo() = 0;

            /**
             * Prints detailed information about the context to the specified output stream.
             * @param out the output stream.
             */
            virtual void printInfo(epics::pvData::StringBuilder out) = 0;
        };


        /**
         * Listener for connect state changes.
         */
        class ChannelRequester : public virtual epics::pvData::Requester {
            public:
            POINTER_DEFINITIONS(ChannelRequester);

            /**
             * A channel has been created. This may be called multiple times if there are multiple providers.
             * @param status Completion status.
             * @param channel The channel.
             */
            virtual void channelCreated(const epics::pvData::Status& status, Channel::shared_pointer const & channel) = 0;

            /**
             * A channel connection state change has occurred.
             * @param c The channel.
             * @param connectionState The new connection state.
             */
            virtual void channelStateChange(Channel::shared_pointer const & channel, Channel::ConnectionState connectionState) = 0;
        };

        /**
         * @brief The FlushStrategy enum
         */
        enum FlushStrategy {
            IMMEDIATE, DELAYED, USER_CONTROLED
        };

        /**
         * Interface implemented by code that can provide access to the record
         * to which a channel connects.
         */
        class ChannelProvider : public epics::pvData::Destroyable, private epics::pvData::NoDefaultMethods {
        public:
            POINTER_DEFINITIONS(ChannelProvider);

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
            virtual ChannelFind::shared_pointer channelFind(epics::pvData::String const & channelName,
                                                     ChannelFindRequester::shared_pointer const & channelFindRequester) = 0;

            /**
             * Create a channel.
             * @param channelName The name of the channel.
             * @param channelRequester The epics::pvData::Requester.
             * @param priority channel priority, must be <code>PRIORITY_MIN</code> <= priority <= <code>PRIORITY_MAX</code>.
             * @return <code>Channel</code> instance. If channel does not exist <code>null</code> is returned and <code>channelRequester</code> notified.
             */
            virtual Channel::shared_pointer createChannel(epics::pvData::String const & channelName,ChannelRequester::shared_pointer const & channelRequester,
                                                   short priority = PRIORITY_DEFAULT) = 0;

            /**
             * Create a channel.
             * @param channelName The name of the channel.
             * @param channelRequester The epics::pvData::Requester.
             * @param priority channel priority, must be <code>PRIORITY_MIN</code> <= priority <= <code>PRIORITY_MAX</code>.
             * @param address address (or list of addresses) where to look for a channel. Implementation independed epics::pvData::String.
             * @return <code>Channel</code> instance. If channel does not exist <code>null</code> is returned and <code>channelRequester</code> notified.
             */
            virtual Channel::shared_pointer createChannel(epics::pvData::String const & channelName,ChannelRequester::shared_pointer const & channelRequester,
                                                   short priority, epics::pvData::String const & address) = 0;

            virtual void configure(epics::pvData::PVStructure::shared_pointer /*configuration*/) {};
            virtual void flush() {};
            virtual void poll() {};

        };

        /**
         * <code>ChanneProvider</code> factory interface.
         */
        class ChannelProviderFactory : private epics::pvData::NoDefaultMethods {
        public:
            POINTER_DEFINITIONS(ChannelProviderFactory);

            /**
             * Get factory name (i.e. name of the provider).
             * @return the factory name.
             */
            virtual epics::pvData::String getFactoryName() = 0;

            /**
             * Get a shared instance.
             * @return a shared instance.
             */
            virtual ChannelProvider::shared_pointer sharedInstance() = 0;

            /**
             * Create a new instance.
             * @return a new instance.
             */
            virtual ChannelProvider::shared_pointer newInstance() = 0;
        };

        /**
         * Interface for locating channel providers.
         */
        class ChannelAccess : private epics::pvData::NoDefaultMethods {
        public:
            POINTER_DEFINITIONS(ChannelAccess);

            typedef std::vector<epics::pvData::String> stringVector_t;
            
            virtual ~ChannelAccess() {};
            
            /**
             * Get a shared instance of the provider with the specified name.
             * @param providerName The name of the provider.
             * @return The interface for the provider or null if the provider is not known.
             */
            virtual ChannelProvider::shared_pointer getProvider(epics::pvData::String const & providerName) = 0;
            
            /**
             * Creates a new instanceof the provider with the specified name.
             * @param providerName The name of the provider.
             * @return The interface for the provider or null if the provider is not known.
             */
            virtual ChannelProvider::shared_pointer createProvider(epics::pvData::String const & providerName) = 0;

            /**
             * Get a array of the names of all the known providers.
             * @return The names. Be sure to delete vector instance.
             */
            virtual std::auto_ptr<stringVector_t> getProviderNames() = 0;
        };
    
        extern ChannelAccess::shared_pointer getChannelAccess();
        extern void registerChannelProviderFactory(ChannelProviderFactory::shared_pointer const & channelProviderFactory);
        extern void unregisterChannelProviderFactory(ChannelProviderFactory::shared_pointer const & channelProviderFactory);

        /**
         * Interface for creating request structure.
         */
        class CreateRequest {
            public:
            POINTER_DEFINITIONS(CreateRequest);

            virtual ~CreateRequest() {};

            /**
             * Create a request structure for the create calls in Channel.
             * See the package overview documentation for details.
             * @param request The field request. See the package overview documentation for details.
             * @param requester The requester;
             * @return The request structure if an invalid request was given. 
             */
             virtual epics::pvData::PVStructure::shared_pointer createRequest(
                 epics::pvData::String const & request,
                 epics::pvData::Requester::shared_pointer const & requester) = 0;
        };

        extern CreateRequest::shared_pointer getCreateRequest();


    }}

#endif  /* PVACCESS_H */
