/* pva.h */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2015.02
 */
#ifndef PVA_H
#define PVA_H

#ifdef epicsExportSharedSymbols
#   define pvaEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <list>
#include <iostream>
#include <pv/requester.h>
#include <pv/status.h>
#include <pv/event.h>
#include <pv/lock.h>
#include <pv/pvData.h>
#include <pv/pvCopy.h>
#include <pv/pvTimeStamp.h>
#include <pv/timeStamp.h>
#include <pv/pvAlarm.h>
#include <pv/alarm.h>
#include <pv/pvAccess.h>
#include <pv/standardField.h>
#include <pv/standardPVField.h>
#include <pv/createRequest.h>
#include <pv/nt.h>

#ifdef pvaEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef pvaEpicsExportSharedSymbols
#endif

#include <shareLib.h>


namespace epics { namespace pva { 

class Pva;
typedef std::tr1::shared_ptr<Pva> PvaPtr;
class PvaGetData;
typedef std::tr1::shared_ptr<PvaGetData> PvaGetDataPtr;
class PvaPutData;
typedef std::tr1::shared_ptr<PvaPutData> PvaPutDataPtr;
class PvaMonitorData;
typedef std::tr1::shared_ptr<PvaMonitorData> PvaMonitorDataPtr;
class PvaChannel;
typedef std::tr1::shared_ptr<PvaChannel> PvaChannelPtr;
class PvaField;
typedef std::tr1::shared_ptr<PvaField> PvaFieldPtr;
class PvaProcess;
typedef std::tr1::shared_ptr<PvaProcess> PvaProcessPtr;
class PvaGet;
typedef std::tr1::shared_ptr<PvaGet> PvaGetPtr;
class PvaPut;
typedef std::tr1::shared_ptr<PvaPut> PvaPutPtr;
class PvaPutGet;
typedef std::tr1::shared_ptr<PvaPutGet> PvaPutGetPtr;
class PvaMonitor;
typedef std::tr1::shared_ptr<PvaMonitor> PvaMonitorPtr;
class PvaMonitorRequester;
typedef std::tr1::shared_ptr<PvaMonitorRequester> PvaMonitorRequesterPtr;
class PvaArray;
typedef std::tr1::shared_ptr<PvaArray> PvaArrayPtr;
class PvaRPC;
typedef std::tr1::shared_ptr<PvaRPC> PvaRPCPtr;

typedef epics::pvData::shared_vector<const PvaChannelPtr> PvaChannelArray;
typedef std::tr1::shared_ptr<PvaChannelArray> PvaChannelArrayPtr;
typedef std::tr1::weak_ptr<PvaChannelArray> PvaChannelArrayWPtr;

class PvaMultiChannel;
typedef std::tr1::shared_ptr<PvaMultiChannel> PvaMultiChannelPtr;
class PvaMultiChannelGet;

// following are private to pva
class PvaChannelCache;
typedef std::tr1::shared_ptr<PvaChannelCache> PvaChannelCachePtr;

/**
 * @brief Pva is a synchronous interface to pvAccess plus convenience methods.
 *
 * @author mrk
 */
class epicsShareClass Pva :
     public epics::pvData::Requester,
     public std::tr1::enable_shared_from_this<Pva>
{
public:
    POINTER_DEFINITIONS(Pva);

    /**
     * Destructor
     */
    ~Pva();
    /**
     * @brief Create an instance of Pva
     * @return shared_ptr to new instance.
     */
    static PvaPtr create();
    /** @brief get the requester name.
     * @return The name.
     */
    std::string getRequesterName();
    /**
     * @brief A new message.
     * If a requester is set then it is called otherwise message is displayed
     * on standard out.
     * @param message The message.
     * @param messageType The type.
     */
    void message(
        std::string const & message,
        epics::pvData::MessageType messageType);
    /**
     * @brief Destroy all the channels and multiChannels.
     */
    void destroy();
    /**
     * @brief get a cached channel or create and connect to a new channel.
     * The provider is pva. The timeout is 5 seconds.
     * If connection can not be made an exception is thrown.
     * @param channelName The channelName.
     * @return The interface.
     */
    PvaChannelPtr channel(std::string const & channelName)
    { return channel(channelName,"pva", 5.0); }
    /**
     * @brief get a cached channel or create and connect to a new channel.
     * If connection can not be made an exception is thrown.
     * @param channelName The channelName.
     * @return The interface.
     */
    PvaChannelPtr channel(
        std::string const & channelName,
        std::string const &providerName,
        double timeOut);
    /**
     * @brief Create an PvaChannel. The provider is pva.
     * @param channelName The channelName.
     * @return The interface.
     */
    PvaChannelPtr createChannel(std::string const & channelName);
    /**
     * @brief Create an PvaChannel with the specified provider.
     * @param channelName The channelName.
     * @param providerName The provider.
     * @return The interface or null if the provider does not exist.
     */
    PvaChannelPtr createChannel(
       std::string const & channelName,
       std::string const & providerName);
    /**
     * @brief Create an PvaMultiChannel. The provider is pvAccess.
     * @param channelName The channelName array.
     * @return The interface.
     */
    PvaMultiChannelPtr createMultiChannel(
        epics::pvData::PVStringArrayPtr const & channelNames);
    /**
     * @brief Create an PvaMultiChannel with the specified provider.
     * @param channelName The channelName array.
     * @param providerName The provider.
     * @return The interface.
     */
    PvaMultiChannelPtr createMultiChannel(
        epics::pvData::PVStringArrayPtr const & channelNames,
        std::string const & providerName);
    /**
     * @brief Set a requester.
     * The default is for Pva to handle messages by printing to System.out.
     * @param requester The requester.
     */
    void setRequester(epics::pvData::RequesterPtr const & requester);
    /**
     * @brief Clear the requester. Pva will handle messages.
     */
    void clearRequester();
    /**
     * @brief get shared pointer to this
     */
    PvaPtr getPtrSelf()
    {
        return shared_from_this();
    }
private:
    Pva();
    PvaChannelCachePtr pvaChannelCache;

    epics::pvData::PVStructurePtr createRequest(std::string const &request);
    std::list<PvaChannelPtr> channelList;
    std::list<PvaMultiChannelPtr> multiChannelList;
    epics::pvData::Requester::weak_pointer requester;
    bool isDestroyed;
    epics::pvData::Mutex mutex;
};

// folowing private to PvaChannel
class PvaGetCache;
typedef std::tr1::shared_ptr<PvaGetCache> PvaGetCachePtr;
class PvaPutCache;
typedef std::tr1::shared_ptr<PvaPutCache> PvaPutCachePtr;
class ChannelRequesterImpl;
/**
 * @brief An easy to use alternative to directly calling the Channel methods of pvAccess.
 *
 * @author mrk
 */
class epicsShareClass PvaChannel :
    public std::tr1::enable_shared_from_this<PvaChannel>
{
public:
    POINTER_DEFINITIONS(PvaChannel);
    /**
     * @brief Create a PvaChannel.
     * @param pva Interface to Pva
     * @param channelName The name of the channel.
     * @return The interface.
     */
    static PvaChannelPtr create(
        PvaPtr const &pva,
        std::string const & channelName)
        {return create(pva,channelName,"pva");}
    /**
     * @brief Create a PvaChannel.
     * @param channelName The name of the channel.
     * @param providerName The name of the provider.
     * @return The interface to the PvaStructure.
     */
    static PvaChannelPtr create(
         PvaPtr const &pva,
         std::string const & channelName,
         std::string const & providerName);
    ~PvaChannel();
    /**
     * @brief Destroy the pvAccess connection.
     */
    void destroy();
    /**
     * @brief Get the name of the channel to which PvaChannel is connected.
     * @return The channel name.
     */
    std::string getChannelName();
    /**
     * @brief Get the the channel to which PvaChannel is connected.
     * @return The channel interface.
     */
    epics::pvAccess::Channel::shared_pointer getChannel();
    /**
     * @brief Connect to the channel.
     * This calls issueConnect and waitConnect.
     * An exception is thrown if connect fails.
     * @param timeout The time to wait for connecting to the channel.
     */
    void connect(double timeout=5.0);
    /**
     * @brief Issue a connect request and return immediately.
     */
    void issueConnect();
    /**
     * @brief Wait until the connection completes or for timeout.
     * @param timeout The time in second to wait.
     * @return status.
     */
    epics::pvData::Status waitConnect(double timeout);
    /**
     * @brief Calls the next method with subField = "";
     * @return The interface.
     */
    PvaFieldPtr createField();
    /**
     * @brief Create an PvaField for the specified subField.
     * @param subField The syntax for subField is defined in package org.epics.pvdata.copy
     * @return The interface.
     */
    PvaFieldPtr createField(std::string const & subField);
    /**
     * @brief Calls the next method with request = "";
     * @return The interface.
     */
    PvaProcessPtr createProcess();
    /**
     * @brief First call createRequest as implemented by pvDataJava and then calls the next method.
     * @param request The request as described in package org.epics.pvdata.copy
     * @return The interface.
     */
    PvaProcessPtr createProcess(std::string const & request);
    /**
     * @brief Creates an PvaProcess. 
     * @param pvRequest The syntax of pvRequest is described in package org.epics.pvdata.copy.
     * @return The interface.
     */
    PvaProcessPtr createProcess(epics::pvData::PVStructurePtr const &  pvRequest);
    /**
     * @brief Call the next method with request =  "field(value,alarm,timeStamp)" 
     * @return The interface.
     */
    PvaGetPtr get();
    /**
     * @brief get a cached PvaGet or create and connect to a new PvaGet.
     * Then call it's get method.
     * If connection can not be made an exception is thrown.
     * @param request The request as described in package org.epics.pvdata.copy
     * @return The interface.
     */
    PvaGetPtr get(std::string const & request);
    /**
     * @brief Call the next method with request =  "field(value,alarm,timeStamp)" 
     * @return The interface.
     */
    PvaGetPtr createGet();
    /**
     * @brief First call createRequest as implemented by pvDataJava and then call the next method.
     * @param request The request as described in package org.epics.pvdata.copy
     * @return The interface.
     */
    PvaGetPtr createGet(std::string const & request);
    /**
     * @brief Creates an PvaGet.
     * @param pvRequest The syntax of pvRequest is described in package org.epics.pvdata.copy.
     * @return The interface.
     */
    PvaGetPtr createGet(epics::pvData::PVStructurePtr const &  pvRequest);
    /**
     * @brief Call the next method with request =  "field(value)" 
     * @return The interface.
     */
    PvaPutPtr put();
    /**
     *  @brief get a cached PvaPut or create and connect to a new PvaPut.
     *  Then call it's get method.
     *  If connection can not be made an exception is thrown.
     *  @param request The request as described in package org.epics.pvdata.copy
     * @return The interface.
     */
    PvaPutPtr put(std::string const & request);
    /**
     *  @brief Call the next method with request = "field(value)" 
     * @return The interface.
     */
    PvaPutPtr createPut();
    /**
     * @brief First call createRequest as implemented by pvDataJava and then calls the next method.
     * @param request The request as described in package org.epics.pvdata.copy
     * @return The interface.
     */
    PvaPutPtr createPut(std::string const & request);
    /**
     * @brief Create an PvaPut.
     * @param pvRequest The syntax of pvRequest is described in package org.epics.pvdata.copy.
     * @return The interface.
     */
    PvaPutPtr createPut(epics::pvData::PVStructurePtr const & pvRequest);
    /**
     *  @brief Call the next method with request = "record[process=true]putField(argument)getField(result)".
     * @return The interface.
     */
    PvaPutGetPtr createPutGet();
    /**
     * @brief First call createRequest as implemented by pvDataJava and then calls the next method.
     * @param request The request as described in package org.epics.pvdata.copy
     * @return The interface.
     */
    PvaPutGetPtr createPutGet(std::string const & request);
    /**
     * @brief Create an PvaPutGet.
     * @param pvRequest The syntax of pvRequest is described in package org.epics.pvdata.copy.
     * @return The interface.
     */
    PvaPutGetPtr createPutGet(epics::pvData::PVStructurePtr const & pvRequest);
    /**
     * @brief Call createRPC(PVStructure(null))
     * @return The interface.
     */
    PvaRPCPtr createRPC();
    /**
     * @brief First call createRequest as implemented by pvDataJava and then calls the next method.
     * @param request The request as described in package org.epics.pvdata.copy
     * @return The interface.
     */
    PvaRPCPtr createRPC(std::string const & request);
    /**
     * @brief Create an PvaRPC.
     * @param pvRequest The syntax of pvRequest is described in package org.epics.pvdata.copy.
     * @return The interface.
     */
    PvaRPCPtr createRPC(epics::pvData::PVStructurePtr const & pvRequest);
    /**
     * @brief Call the next method with request = "field(value)";
     * @return The interface.
     */
    PvaArrayPtr createArray();
    /**
     * @brief First call createRequest as implemented by pvDataJava and then calls the next method.
     * @param request The request as described in package org.epics.pvdata.copy
     * @return The interface.
     */
    PvaArrayPtr createArray(std::string const & request);
    /**
     * @brief Create an PvaArray.
     * @param pvRequest The syntax of pvRequest is described in package org.epics.pvdata.copy.
     * @return The interface.
     */
    PvaArrayPtr createArray(epics::pvData::PVStructurePtr const &  pvRequest);
    /**
     * @brief Call the next method with request =  "field(value,alarm,timeStamp)" 
     * @return The interface.
     */
    PvaMonitorPtr monitor();
    /**
     * @brief get a cached PvaMonitor or create and connect to a new PvaMonitor.
     * Then call it's start method.
     * If connection can not be made an exception is thrown.
     * @param request The request as described in package org.epics.pvdata.copy
     * @return The interface.
     */
    PvaMonitorPtr monitor(std::string const & request);
    /**
      * @brief Call the next method with request =  "field(value,alarm,timeStamp)" 
      * @param pvaMonitorRequester The client callback.
      * @return The interface.
      */
    PvaMonitorPtr monitor(PvaMonitorRequesterPtr const & pvaMonitorRequester);

    /**
     * @brief get a cached PvaMonitor or create and connect to a new PvaMonitor.
     * Then call it's start method.
     * If connection can not be made an exception is thrown.
     * @param request The request as described in package org.epics.pvdata.copy
     * @param pvaMonitorRequester The client callback.
     * @return The interface.
     */
    PvaMonitorPtr monitor(
        std::string const & request,
        PvaMonitorRequesterPtr const & pvaMonitorRequester);
    /**
     * @brief Call the next method with request = "field(value.alarm,timeStamp)" 
     * @return The interface.
     */
    PvaMonitorPtr createMonitor();
    /**
     * @brief First call createRequest as implemented by pvDataJava and then calls the next method.
     * @param request The request as described in package org.epics.pvdata.copy
     * @return The interface.
     */
    PvaMonitorPtr createMonitor(std::string const & request);
    /**
     * @brief Create an PvaMonitor.
     * @param pvRequest  The syntax of pvRequest is described in package org.epics.pvdata.copy.
     * @return The interface.
     */
    PvaMonitorPtr createMonitor(epics::pvData::PVStructurePtr const &  pvRequest);
    PvaChannelPtr getPtrSelf()
    {
        return shared_from_this();
    }
private:
    PvaChannel(
        PvaPtr const &pva,
        std::string const & channelName,
        std::string const & providerName);
    void channelCreated(
        const epics::pvData::Status& status,
        epics::pvAccess::Channel::shared_pointer const & channel);
    void channelStateChange(
        epics::pvAccess::Channel::shared_pointer const & channel,
        epics::pvAccess::Channel::ConnectionState connectionState);
    std::string getRequesterName();
    void message(
        std::string const & message,
        epics::pvData::MessageType messageType);

    enum ConnectState {connectIdle,connectActive,notConnected,connected};

    Pva::weak_pointer pva;
    std::string channelName;
    std::string providerName;
    ConnectState connectState;
    bool isDestroyed;
    epics::pvData::CreateRequest::shared_pointer createRequest;
    PvaGetCachePtr pvaGetCache;
    PvaPutCachePtr pvaPutCache;

    epics::pvData::Status channelConnectStatus;
    epics::pvData::Mutex mutex;
    epics::pvData::Event waitForConnect;
    epics::pvAccess::Channel::shared_pointer channel;
    epics::pvAccess::ChannelRequester::shared_pointer channelRequester;
    friend class ChannelRequesterImpl;
};

/**
 * @brief This is a class that holds data returned by PvaGet or PvaPutGet
 *
 */
class epicsShareClass PvaGetData
{
public:
    POINTER_DEFINITIONS(PvaGetData);
    /**
     * @brief Factory method for creating an instance of PvaGetData.
     */
    static PvaGetDataPtr create(epics::pvData::StructureConstPtr const & structure);
    ~PvaGetData() {}
    /**
     * @brief Set a prefix for throw messages.
     * @param value The prefix.
     */
    void setMessagePrefix(std::string const & value);
   /** @brief Get the structure.
    * @return the structure.
    */
   epics::pvData::StructureConstPtr getStructure();
   /** @brief Get the pvStructure.
    * @return the pvStructure.
    */
   epics::pvData::PVStructurePtr getPVStructure();
   /** @brief Get the BitSet for the pvStructure
    * This shows which fields have changed value.
    * @return The bitSet
    */
   epics::pvData::BitSetPtr getBitSet();
   /** @brief show the fields that have changed.
    * @param out The stream that shows the changed fields.
    * @return The stream that was input
    */
   std::ostream & showChanged(std::ostream & out);
    /**
     * @brief New data is present.
     * @param pvStructureFrom The new data.
     * @param bitSetFrom the bitSet showing which values have changed.
     */
    void setData(
        epics::pvData::PVStructurePtr const & pvStructureFrom,
        epics::pvData::BitSetPtr const & bitSetFrom);
    /**
     * @brief Is there a top level field named value.
     * @return The answer.
     */
    bool hasValue();
    /**
     * @brief Is the value field a scalar?
     * @return The answer.
     */
    bool isValueScalar();
    /**
     * @brief Is the value field a scalar array?
     * @return The answer.
     */
    bool isValueScalarArray();
    /**
     * @brief Return the interface to the value field.
     * @return The interface. an excetion is thrown if a value field does not exist.
     */
    epics::pvData::PVFieldPtr getValue();
    /**
     * @brief Return the interface to a scalar value field.
     * @return The interface for a scalar value field.
     * An exception is thown if no scalar value field.
     */
    epics::pvData::PVScalarPtr getScalarValue();
    /**
     * @brief Return the interface to an array value field.
     * @return The interface.
     * An exception is thown if no array value field.
     */
    std::tr1::shared_ptr<epics::pvData::PVArray> getArrayValue();
    /**
     * @brief Return the interface to a scalar array value field.
     * @return Return the interface.
     * An exception is thown if no scalar array value field.
     */
    std::tr1::shared_ptr<epics::pvData::PVScalarArray> getScalarArrayValue();
    /**
     * @brief Get the value as a double.
     * If value is not a numeric scalar an exception is thrown.
     * @return The value.
     */
    double getDouble();
    /**
     * @brief Get the value as a string.
     * If value is not a scalar an exception is thrown
     * @return The value.
     */
    std::string getString();
    /**
     * @brief Get the value as a double array.
     * If the value is not a numeric array an exception is thrown.
     * @return The value.
     */
    epics::pvData::shared_vector<const double>  getDoubleArray();
    /**
     * @brief Get the value as a string array.
     * If the value is not a string array an exception is thrown.
     * @return The value.
     */
    epics::pvData::shared_vector<const std::string>  getStringArray();
    /**
     * @brief Get the alarm.
     * If the pvStructure as an alarm field it's values are returned.
     * If no then alarm shows that not alarm defined.
     * @return The alarm.
     */
    epics::pvData::Alarm getAlarm();
    /**
     * @brief Get the timeStamp.
     * If the pvStructure as a timeStamp field, it's values are returned.
     * If no then all fields are 0.
     * @return The timeStamp.
     */
    epics::pvData::TimeStamp getTimeStamp();
private:
    PvaGetData(epics::pvData::StructureConstPtr const & structure);
    void checkValue();
    epics::pvData::StructureConstPtr structure;
    epics::pvData::PVStructurePtr pvStructure;
    epics::pvData::BitSetPtr bitSet;

    std::string messagePrefix;
    epics::pvData::PVFieldPtr pvValue;
    epics::pvData::PVAlarm pvAlarm;
    epics::pvData::PVTimeStamp pvTimeStamp;
};

class PvaPostHandlerPvt; // private to PvaPutData
/**
 * @brief This is a class that holds data given to  by PvaPut or PvaPutGet
 *
 */
class epicsShareClass PvaPutData
{
public:
    POINTER_DEFINITIONS(PvaPutData);
    /**
     * @brief Factory method for creating an instance of PvaPutData.
     */
    static PvaPutDataPtr create(epics::pvData::StructureConstPtr const & structure);
    ~PvaPutData() {}
    /**
     * @brief Set a prefix for throw messages.
     * @param value The prefix.
     */
    void setMessagePrefix(std::string const & value);
   /** @brief Get the structure.
    * @return the structure.
    */
   epics::pvData::StructureConstPtr getStructure();
    /** @brief Get the pvStructure.
     * @return the pvStructure.
     */
    epics::pvData::PVStructurePtr getPVStructure();
    /** @brief Get the BitSet for the pvStructure
     * This shows which fields have changed value.
     * @return The bitSet
     */
    epics::pvData::BitSetPtr getBitSet();
    /** @brief show the fields that have changed.
     * @param out The stream that shows the changed fields.
     * @return The stream that was input
     */
    std::ostream & showChanged(std::ostream & out);
    /**
     * @brief Is there a top level field named value.
     * @return The answer.
     */
    bool hasValue();
    /**
     * @brief Is the value field a scalar?
     * @return The answer.
     */
    bool isValueScalar();
    /**
     * @brief Is the value field a scalar array?
     * @return The answer.
     */
    bool isValueScalarArray();
    /**
     * @brief Return the interface to the value field.
     * @return The interface. an excetion is thrown if a value field does not exist.
     */
    epics::pvData::PVFieldPtr getValue();
    /**
     * @brief Return the interface to a scalar value field.
     * @return The interface for a scalar value field.
     * An exception is thown if no scalar value field.
     */
    epics::pvData::PVScalarPtr getScalarValue();
    /**
     * @brief Return the interface to an array value field.
     * @return The interface.
     * An exception is thown if no array value field.
     */
    std::tr1::shared_ptr<epics::pvData::PVArray> getArrayValue();
    /**
     * @brief Return the interface to a scalar array value field.
     * @return Return the interface.
     * An exception is thown if no scalar array value field.
     */
    std::tr1::shared_ptr<epics::pvData::PVScalarArray> getScalarArrayValue();
    /**
     * @brief Get the value as a double.
     * If value is not a numeric scalar an exception is thrown.
     * @return The value.
     */
    double getDouble();
    /**
     * @brief Get the value as a string.
     * If value is not a string an exception is thrown
     * @return The value.
     */
    std::string getString();
    /**
     * @brief Get the value as a double array.
     * If the value is not a numeric array an exception is thrown.
     * @return The value.
     */
    epics::pvData::shared_vector<const double>  getDoubleArray();
    /**
     * @brief Get the value as a string array.
     * If the value is not a string array an exception is thrown.
     * @return The value.
     */
    epics::pvData::shared_vector<const std::string>  getStringArray();
    /**
     * Put the value as a double.
     * An exception is also thrown if the actualy type can cause an overflow.
     * If value is not a numeric scalar an exception is thrown.
     */
    void putDouble(double value);
    /**
     * Put the value as a string.
     * If value is not a  scalar an exception is thrown.
     */
    void putString(std::string const & value);
    /**
     * Copy the array to the value field.
     * If the value field is not a double array field an exception is thrown.
     * @param value The place where data is copied.
     */
    void putDoubleArray(epics::pvData::shared_vector<const double> const & value);
    /**
     * Copy array to the value field.
     * If the value field is not a string array field an exception is thrown.
     * @param value data source
     */
    void putStringArray(epics::pvData::shared_vector<const std::string> const & value);
    /**
     * Copy array to the value field.
     * If the value field is not a scalarArray field an exception is thrown.
     * @param value data source
     */
    void putStringArray(std::vector<std::string> const & value);
private:
    PvaPutData(epics::pvData::StructureConstPtr const &structure);
    void checkValue();
    void postPut(size_t fieldNumber);

    std::vector<epics::pvData::PostHandlerPtr> postHandler;
    epics::pvData::StructureConstPtr structure;
    epics::pvData::PVStructurePtr pvStructure;
    epics::pvData::BitSetPtr bitSet;
    friend class PvaPostHandlerPvt;

    std::string messagePrefix;
    epics::pvData::PVFieldPtr pvValue;
};

/**
 * @brief This is a class that holds data returned by PvaMonitor
 *
 */
class epicsShareClass PvaMonitorData
{
public:
    POINTER_DEFINITIONS(PvaMonitorData);
    /**
     * @brief Factory method for creating an instance of PvaMonitorData.
     */
    static PvaMonitorDataPtr create(epics::pvData::StructureConstPtr const & structure);
    ~PvaMonitorData() {}
    /**
     * @brief Set a prefix for throw messages.
     * @param value The prefix.
     */
    void setMessagePrefix(std::string const & value);
   /** @brief Get the structure.
    * @return the structure.
    */
   epics::pvData::StructureConstPtr getStructure();
    /** @brief Get the pvStructure.
     * @return the pvStructure.
     */
    epics::pvData::PVStructurePtr getPVStructure();
    /** @brief Get the BitSet for the pvStructure
     * This shows which fields have changed value.
     * @return The bitSet
     */
    epics::pvData::BitSetPtr getChangedBitSet();
    /** @brief Get the overrun BitSet for the pvStructure
     * This shows which fields have had more than one change.
     * @return The bitSet
     */
    epics::pvData::BitSetPtr getOverrunBitSet();
    /** @brief show the fields that have changed.
     * @param out The stream that shows the changed fields.
     * @return The stream that was input
     */
    std::ostream & showChanged(std::ostream & out);
    /** @brief show the fields that have overrun.
     * @param out The stream that shows the overrun fields.
     * @return The stream that was input
     */
    std::ostream & showOverrun(std::ostream & out);
    /**
     * @brief New data is present.
     * @param monitorElement The new data.
     */
    void setData(epics::pvData::MonitorElementPtr const & monitorElement);
    /**
     * @brief Is there a top level field named value.
     * @return The answer.
     */
    bool hasValue();
    /**
     * @brief Is the value field a scalar?
     * @return The answer.
     */
    bool isValueScalar();
    /**
     * @brief Is the value field a scalar array?
     * @return The answer.
     */
    bool isValueScalarArray();
    /**
     * @brief Return the interface to the value field.
     * @return The interface. an excetion is thrown if a value field does not exist.
     */
    epics::pvData::PVFieldPtr getValue();
    /**
     * @brief Return the interface to a scalar value field.
     * @return The interface for a scalar value field.
     * An exception is thown if no scalar value field.
     */
    epics::pvData::PVScalarPtr getScalarValue();
    /**
     * @brief Return the interface to an array value field.
     * @return The interface.
     * An exception is thown if no array value field.
     */
    std::tr1::shared_ptr<epics::pvData::PVArray> getArrayValue();
    /**
     * @brief Return the interface to a scalar array value field.
     * @return Return the interface.
     * An exception is thown if no scalar array value field.
     */
    std::tr1::shared_ptr<epics::pvData::PVScalarArray> getScalarArrayValue();
    /**
     * @brief Get the value as a double.
     * If value is not a numeric scalar an exception is thrown.
     * @return The value.
     */
    double getDouble();
    /**
     * @brief Get the value as a string.
     * If value is not a scalar an exception is thrown
     * @return The value.
     */
    std::string getString();
    /**
     * @brief Get the value as a double array.
     * If the value is not a numeric array an exception is thrown.
     * @return The value.
     */
    epics::pvData::shared_vector<const double>  getDoubleArray();
    /**
     * @brief Get the value as a string array.
     * If the value is not a string array an exception is thrown.
     * @return The value.
     */
    epics::pvData::shared_vector<const std::string>  getStringArray();
    /**
     * @brief Get the alarm.
     * If the pvStructure as an alarm field it's values are returned.
     * If no then alarm shows that not alarm defined.
     * @return The alarm.
     */
    epics::pvData::Alarm getAlarm();
    /**
     * @brief Get the timeStamp.
     * If the pvStructure as a timeStamp field, it's values are returned.
     * If no then all fields are 0.
     * @return The timeStamp.
     */
    epics::pvData::TimeStamp getTimeStamp();
private:
    PvaMonitorData(epics::pvData::StructureConstPtr const & structure);
    void checkValue();

    epics::pvData::StructureConstPtr structure;
    epics::pvData::PVStructurePtr pvStructure;
    epics::pvData::BitSetPtr changedBitSet;
    epics::pvData::BitSetPtr overrunBitSet;

    std::string messagePrefix;
    epics::pvData::PVFieldPtr pvValue;
    epics::pvData::PVAlarm pvAlarm;
    epics::pvData::PVTimeStamp pvTimeStamp;
};

class ChannelProcessRequesterImpl; // private to PvaProcess
/**
 * @brief An easy to use alternative to ChannelProcess.
 *
 * @author mrk
 */
class epicsShareClass PvaProcess 
{
public:
    POINTER_DEFINITIONS(PvaProcess);
    /**
     * @brief Create a PvaProcess.
     * @param &pva Interface to Pva
     * @param pvaChannel Interface to PvaChannel
     * @param channel Interface to Channel
     * @param pvRequest The request structure.
     * @return The interface to the PvaStructure.
     */
    static PvaProcessPtr create(
        PvaPtr const &pva,
        PvaChannelPtr const & pvaChannel,
        epics::pvAccess::Channel::shared_pointer const & channel,
        epics::pvData::PVStructurePtr const &pvRequest
    );
    /**
     * @brief destructor
     */
    ~PvaProcess();
    /** 
     * @brief destroy an resources used.
     */
    void destroy();
    /**
     * @brief call issueConnect and then waitConnect.
     * An exception is thrown if connect fails.
     */
    void connect();
    /**
     * @brief create the channelProcess connection to the channel.
     * This can only be called once.
     */
    void issueConnect();
    /**
     * @brief wait until the channelProcess connection to the channel is complete.
     * @return status;
     */
    epics::pvData::Status waitConnect();
    /**
     * @brief Call issueProcess and then waitProcess.
     * An exception is thrown if get fails.
     */
    void process();
    /**
     * @brief Issue a get and return immediately.
     */
    void issueProcess();
    /**
     * @brief Wait until get completes.
     * @return status.
     */
    epics::pvData::Status waitProcess();
private:
    PvaProcess(
        PvaPtr const &pva,
        PvaChannelPtr const & pvaChannel,
        epics::pvAccess::Channel::shared_pointer const & channel,
        epics::pvData::PVStructurePtr const &pvRequest);
    std::string getRequesterName();
    void message(std::string const & message,epics::pvData::MessageType messageType);
    void channelProcessConnect(
        const epics::pvData::Status& status,
        epics::pvAccess::ChannelProcess::shared_pointer const & channelProcess);
    void processDone(
        const epics::pvData::Status& status,
        epics::pvAccess::ChannelProcess::shared_pointer const & channelProcess);
    void checkProcessState();
    enum ProcessConnectState {connectIdle,connectActive,connected};

    Pva::weak_pointer pva;
    PvaChannel::weak_pointer pvaChannel;
    epics::pvAccess::Channel::shared_pointer channel;
    epics::pvAccess::ChannelProcessRequester::shared_pointer processRequester;
    epics::pvData::PVStructurePtr pvRequest;
    epics::pvData::Mutex mutex;
    epics::pvData::Event waitForConnect;
    epics::pvData::Event waitForProcess;
    std::string messagePrefix;

    bool isDestroyed;
    epics::pvData::Status channelProcessConnectStatus;
    epics::pvData::Status channelProcessStatus;
    epics::pvAccess::ChannelProcess::shared_pointer channelProcess;

    ProcessConnectState connectState;

    enum ProcessState {processIdle,processActive,processComplete};
    ProcessState processState;
    friend class ChannelProcessRequesterImpl;
};

class ChannelGetRequesterImpl; // private to PvaGet
/**
 * @brief An easy to use alternative to ChannelGet.
 *
 * @author mrk
 */
class epicsShareClass PvaGet 
{
public:
    POINTER_DEFINITIONS(PvaGet);
    /**
     * @brief Create a PvaGet.
     * @param &pva Interface to Pva
     * @param pvaChannel Interface to PvaChannel
     * @param channel Interface to Channel
     * @param pvRequest The request structure.
     * @return The interface to the PvaStructure.
     */
    static PvaGetPtr create(
        PvaPtr const &pva,
        PvaChannelPtr const & pvaChannel,
        epics::pvAccess::Channel::shared_pointer const & channel,
        epics::pvData::PVStructurePtr const &pvRequest
    );
    /**
     * @brief destructor
     */
    ~PvaGet();
    /** 
     * @brief destroy an resources used.
     */
    void destroy();
    /**
     * @brief call issueConnect and then waitConnect.
     * An exception is thrown if connect fails.
     */
    void connect();
    /**
     * @brief create the channelGet connection to the channel.
     * This can only be called once.
     */
    void issueConnect();
    /**
     * @brief wait until the channelGet connection to the channel is complete.
     * @return status;
     */
    epics::pvData::Status waitConnect();
    /**
     * @brief Call issueGet and then waitGet.
     * An exception is thrown if get fails.
     */
    void get();
    /**
     * @brief Issue a get and return immediately.
     */
    void issueGet();
    /**
     * @brief Wait until get completes.
     * @return status;
     */
    epics::pvData::Status waitGet();
    /**
     * @brief Get the data/
     * @return The interface.
     */
    PvaGetDataPtr getData();   
private:
    PvaGet(
        PvaPtr const &pva,
        PvaChannelPtr const & pvaChannel,
        epics::pvAccess::Channel::shared_pointer const & channel,
        epics::pvData::PVStructurePtr const &pvRequest);
    std::string getRequesterName();
    void message(std::string const & message,epics::pvData::MessageType messageType);
    void channelGetConnect(
        const epics::pvData::Status& status,
        epics::pvAccess::ChannelGet::shared_pointer const & channelGet,
        epics::pvData::StructureConstPtr const & structure);
    void getDone(
        const epics::pvData::Status& status,
        epics::pvAccess::ChannelGet::shared_pointer const & channelGet,
        epics::pvData::PVStructurePtr const & pvStructure,
        epics::pvData::BitSetPtr const & bitSet);
    void checkGetState();
    enum GetConnectState {connectIdle,connectActive,connected};

    Pva::weak_pointer pva;
    PvaChannel::weak_pointer pvaChannel;
    epics::pvAccess::Channel::shared_pointer channel;
    epics::pvAccess::ChannelGetRequester::shared_pointer getRequester;
    epics::pvData::PVStructurePtr pvRequest;
    epics::pvData::Mutex mutex;
    epics::pvData::Event waitForConnect;
    epics::pvData::Event waitForGet;
    PvaGetDataPtr pvaData;
    std::string messagePrefix;

    bool isDestroyed;
    epics::pvData::Status channelGetConnectStatus;
    epics::pvData::Status channelGetStatus;
    epics::pvAccess::ChannelGet::shared_pointer channelGet;

    GetConnectState connectState;

    enum GetState {getIdle,getActive,getComplete};
    GetState getState;
    friend class ChannelGetRequesterImpl;
};

class ChannelPutRequesterImpl; // private to PvaPut
/**
 * @brief An easy to use alternative to ChannelPut.
 *
 * @author mrk
 */
class epicsShareClass PvaPut 
{
public:
    POINTER_DEFINITIONS(PvaPut);
    /**
     * @brief Create a PvaPut.
     * @param &pva Interface to Pva
     * @param pvaChannel Interface to PvaChannel
     * @param channel Interface to Channel
     * @param pvRequest The request structure.
     * @return The interface to the PvaStructure.
     */
    static PvaPutPtr create(
        PvaPtr const &pva,
        PvaChannelPtr const & pvaChannel,
        epics::pvAccess::Channel::shared_pointer const & channel,
        epics::pvData::PVStructurePtr const &pvRequest
    );
    /**
     * @brief destructor
     */
    ~PvaPut();
    /** 
     * @brief destroy an resources used.
     */
    void destroy();
    /**
     * @brief call issueConnect and then waitConnect.
     * An exception is thrown if connect fails.
     */
    void connect();
    /**
     * @brief create the channelPut connection to the channel.
     * This can only be called once.
     */
    void issueConnect();
    /**
     * @brief wait until the channelPut connection to the channel is complete.
     * @return status;
     */
    epics::pvData::Status waitConnect();
    /**
     * @brief Call issueGet and then waitGet.
     * An exception is thrown if get fails.
     */
    void get();
    /**
     * @brief Issue a get and return immediately.
     */
    void issueGet();
    /**
     * @brief Wait until get completes.
     * @return status
     */
    epics::pvData::Status waitGet();
    /**
     * @brief Call issuePut and then waitPut.
     * An exception is thrown if get fails.
     */
    void put();
    /**
     * @brief Issue a put and return immediately.
     */
    void issuePut();
    /**
     * @brief Wait until put completes.
     * @return status
     */
    epics::pvData::Status waitPut();
    /**
     * @brief Get the data/
     * @return The interface.
     */
    PvaPutDataPtr getData();   
private :
    PvaPut(
        PvaPtr const &pva,
        PvaChannelPtr const & pvaChannel,
        epics::pvAccess::Channel::shared_pointer const & channel,
        epics::pvData::PVStructurePtr const &pvRequest);
    std::string getRequesterName();
    void message(std::string const & message,epics::pvData::MessageType messageType);
    void channelPutConnect(
        const epics::pvData::Status& status,
        epics::pvAccess::ChannelPut::shared_pointer const & channelPut,
        epics::pvData::StructureConstPtr const & structure);
    void getDone(
        const epics::pvData::Status& status,
        epics::pvAccess::ChannelPut::shared_pointer const & channelPut,
        epics::pvData::PVStructurePtr const & pvStructure,
        epics::pvData::BitSetPtr const & bitSet);
    void putDone(
        const epics::pvData::Status& status,
        epics::pvAccess::ChannelPut::shared_pointer const & channelPut);
    void checkPutState();
    enum PutConnectState {connectIdle,connectActive,connected};

    Pva::weak_pointer pva;
    PvaChannel::weak_pointer pvaChannel;
    epics::pvAccess::Channel::shared_pointer channel;
    epics::pvAccess::ChannelPutRequester::shared_pointer putRequester;
    epics::pvData::PVStructurePtr pvRequest;
    epics::pvData::Mutex mutex;
    epics::pvData::Event waitForConnect;
    epics::pvData::Event waitForGetPut;
    PvaPutDataPtr pvaData;
    std::string messagePrefix;

    bool isDestroyed;
    epics::pvData::Status channelPutConnectStatus;
    epics::pvData::Status channelGetPutStatus;
    epics::pvAccess::ChannelPut::shared_pointer channelPut;

    PutConnectState connectState;

    enum PutState {putIdle,getActive,putActive,putComplete};
    PutState putState;
    friend class ChannelPutRequesterImpl;
};

class ChannelPutGetRequesterImpl; // private to PvaPutGet
/**
 * @brief An easy to use alternative to ChannelPutGet.
 *
 * @author mrk
 */
class epicsShareClass PvaPutGet 
{
public:
    POINTER_DEFINITIONS(PvaPutGet);
    /**
     * @brief Create a PvaPutGet.
     * @param &pva Interface to Pva
     * @param pvaChannel Interface to PvaChannel
     * @param channel Interface to Channel
     * @param pvRequest The request structure.
     * @return The interface to the PvaStructure.
     */
    static PvaPutGetPtr create(
        PvaPtr const &pva,
        PvaChannelPtr const & pvaChannel,
        epics::pvAccess::Channel::shared_pointer const & channel,
        epics::pvData::PVStructurePtr const &pvRequest
    );
    /**
     * @brief destructor
     */
    ~PvaPutGet();
    /** 
     * @brief destroy an resources used.
     */
    void destroy();
    /**
     * @brief call issueConnect and then waitConnect.
     * An exception is thrown if connect fails.
     */
    void connect();
    /**
     * @brief create the channelPutGet connection to the channel.
     * This can only be called once.
     * An exception is thrown if connect fails.
     */
    void issueConnect();
    /**
     * @brief wait until the channelPutGet connection to the channel is complete.
     * @return status;
     */
    epics::pvData::Status waitConnect();
    /**
     * @brief Call issuePutGet and then waitPutGet.
     * An exception is thrown if putGet fails.
     */
    void putGet();
    /**
     * @brief Issue a putGet and return immediately.
     */
    void issuePutGet();
    /**
     * @brief Wait until putGet completes.
     * If failure getStatus can be called to get reason.
     * @return status
     */
    epics::pvData::Status waitPutGet();
    /**
     * @brief Call issueGet and then waitGetGet.
     * An exception is thrown if get fails.
     */
    void getGet();
    /**
     * @brief Issue a getGet and return immediately.
     */
    void issueGetGet();
    /**
     * @brief Wait until getGet completes.
     * If failure getStatus can be called to get reason.
     * @return status
     */
    epics::pvData::Status waitGetGet();
    /**
     * @brief Call issuePut and then waitGetPut.
     * An exception is thrown if getPut fails.
     */
    void getPut();
    /**
     * @brief Issue a getPut and return immediately.
     */
    void issueGetPut();
    /**
     * @brief Wait until getPut completes.
     * @return status
     */
    epics::pvData::Status waitGetPut();
    /**
     * @brief Get the put data.
     * @return The interface.
     */
    PvaPutDataPtr getPutData();   
    /**
     * @brief Get the get data.
     * @return The interface.
     */
    PvaGetDataPtr getGetData();   
private :
    PvaPutGet(
        PvaPtr const &pva,
        PvaChannelPtr const & pvaChannel,
        epics::pvAccess::Channel::shared_pointer const & channel,
        epics::pvData::PVStructurePtr const &pvRequest);
    std::string getRequesterName();
    void message(std::string const & message,epics::pvData::MessageType messageType);
    void channelPutGetConnect(
        const epics::pvData::Status& status,
        epics::pvAccess::ChannelPutGet::shared_pointer const & channelPutGet,
        epics::pvData::StructureConstPtr const & putStructure,
        epics::pvData::StructureConstPtr const & getStructure);
    void putGetDone(
        const epics::pvData::Status& status,
        epics::pvAccess::ChannelPutGet::shared_pointer const & channelPutGet,
        epics::pvData::PVStructurePtr const & getPVStructure,
        epics::pvData::BitSetPtr const & getBitSet);
    void getPutDone(
        const epics::pvData::Status& status,
        epics::pvAccess::ChannelPutGet::shared_pointer const & channelPutGet,
        epics::pvData::PVStructurePtr const & putPVStructure,
        epics::pvData::BitSet::shared_pointer const & putBitSet);
    void getGetDone(
        const epics::pvData::Status& status,
        epics::pvAccess::ChannelPutGet::shared_pointer const & channelPutGet,
        epics::pvData::PVStructurePtr const & getPVStructure,
        epics::pvData::BitSet::shared_pointer const & getBitSet);
    void checkPutGetState();
    enum PutGetConnectState {connectIdle,connectActive,connected};

    Pva::weak_pointer pva;
    PvaChannel::weak_pointer pvaChannel;
    epics::pvAccess::Channel::shared_pointer channel;
    epics::pvAccess::ChannelPutGetRequester::shared_pointer putGetRequester;
    epics::pvData::PVStructurePtr pvRequest;
    epics::pvData::Mutex mutex;
    epics::pvData::Event waitForConnect;
    epics::pvData::Event waitForPutGet;
    PvaGetDataPtr pvaGetData;
    PvaPutDataPtr pvaPutData;
    std::string messagePrefix;

    bool isDestroyed;
    epics::pvData::Status channelPutGetConnectStatus;
    epics::pvData::Status channelGetPutGetStatus;
    epics::pvAccess::ChannelPutGet::shared_pointer channelPutGet;

    PutGetConnectState connectState;
    epics::pvData::Status channelPutGetStatus;

    enum PutGetState {putGetIdle,putGetActive,putGetComplete};
    PutGetState putGetState;
    friend class ChannelPutGetRequesterImpl;
};

class ChannelMonitorRequester; // private to PvaMonitor
/**
 * @brief Optional client callback.
 *
 */
class epicsShareClass PvaMonitorRequester
{
public:
    POINTER_DEFINITIONS(PvaMonitorRequester);
    /**
     * @brief destructor
     */
    virtual ~PvaMonitorRequester(){}
    /**
     * @brief A monitor event has occurred.
     * @param monitor The PvaMonitor that received the event.
     */
    virtual void event(PvaMonitorPtr monitor) = 0;
};

/**
 * @brief An easy to use alternative to Monitor.
 *
 */
class epicsShareClass PvaMonitor :
     public std::tr1::enable_shared_from_this<PvaMonitor>
{
public:
    POINTER_DEFINITIONS(PvaMonitor);
    /**
     * @brief Create a PvaMonitor.
     * @param &pva Interface to Pva
     * @param pvaChannel Interface to PvaChannel
     * @param channel Interface to Channel
     * @param pvRequest The request structure.
     * @return The interface to the PvaStructure.
     */
    static PvaMonitorPtr create(
        PvaPtr const &pva,
        PvaChannelPtr const & pvaChannel,
        epics::pvAccess::Channel::shared_pointer const & channel,
        epics::pvData::PVStructurePtr const &pvRequest
    );
    /**
     * @brief destructor
     */
    ~PvaMonitor();
    /** 
     * @brief destroy an resources used.
     */
    void destroy();
    /**
     * @brief call issueConnect and then waitConnect.
     * An exception is thrown if connect fails.
     */
    void connect();
    /**
     * @brief create the channelMonitor connection to the channel.
     * This can only be called once.
     * An exception is thrown if connect fails.
     */
    void issueConnect();
    /**
     * @brief wait until the channelMonitor connection to the channel is complete.
     * @return status;
     */
    epics::pvData::Status waitConnect();
    /**
     * @brief Set a user callback.
     * @param pvaMonitorrRequester The requester which must be implemented by the caller.
     */
    void setRequester(PvaMonitorRequesterPtr const & pvaMonitorrRequester);
    /**
     * @brief Start monitoring.
     */
    void start();
    /**
     * @brief Stop monitoring.
     */
    void stop();
    /**
     * @brief poll for a monitor event.
     * The data will be in PvaData.
     * @return (false,true) means event (did not, did) occur.
     */
    bool poll();
    /**
     * @brief wait for a monitor event.
     * The data will be in PvaData.
     * @param secondsToWait Time to wait for event.
     * @return (false,true) means event (did not, did) occur.
     */
    bool waitEvent(double secondsToWait = 0.0);
    /**
     * @brief Release the monitorElement returned by poll
     */
    void releaseEvent();
    /**
     * @brief The data in which monitor events are placed.
     * @return The interface.
     */
    PvaMonitorDataPtr getData();   
    /**
     * @brief get shared pointer to this
     */
    PvaMonitorPtr getPtrSelf()
    {
        return shared_from_this();
    }
private:
    PvaMonitor(
        PvaPtr const &pva,
        PvaChannelPtr const & pvaChannel,
        epics::pvAccess::Channel::shared_pointer const & channel,
        epics::pvData::PVStructurePtr const &pvRequest);
    std::string getRequesterName();
    void message(std::string const & message,epics::pvData::MessageType messageType);
    void monitorConnect(
        const epics::pvData::Status& status,
        epics::pvData::MonitorPtr const & monitor,
        epics::pvData::StructureConstPtr const & structure);
    void monitorEvent(epics::pvData::MonitorPtr const & monitor);
    void unlisten();
    void checkMonitorState();
    enum MonitorConnectState {connectIdle,connectActive,connected,monitorStarted};

    Pva::weak_pointer pva;
    PvaChannel::weak_pointer pvaChannel;
    epics::pvAccess::Channel::shared_pointer channel;
    epics::pvData::PVStructurePtr pvRequest;
    epics::pvData::MonitorRequester::shared_pointer monitorRequester;
    epics::pvData::Mutex mutex;
    epics::pvData::Event waitForConnect;
    epics::pvData::Event waitForEvent;
    PvaMonitorDataPtr pvaData;
    std::string messagePrefix;

    bool isDestroyed;
    epics::pvData::Status connectStatus;
    epics::pvData::MonitorPtr monitor;
    epics::pvData::MonitorElementPtr monitorElement;
    PvaMonitorRequester::weak_pointer pvaMonitorRequester;

    MonitorConnectState connectState;
    bool userPoll;
    bool userWait;
    friend class ChannelMonitorRequester;
};

/**
 * @brief Provides access to multiple channels.
 *
 * @author mrk
 */
class epicsShareClass PvaMultiChannel :
    public std::tr1::enable_shared_from_this<PvaMultiChannel>
{
public:
    POINTER_DEFINITIONS(PvaMultiChannel);
    /**
     * @brief Create a PvaMultiChannel.
     * @param channelNames The name. of the channel..
     * @param providerName The name of the provider.
     * @return The interface to the PvaStructure.
     */
    static PvaMultiChannelPtr create(
         PvaPtr const &pva,
         epics::pvData::PVStringArrayPtr const & channelNames,
         std::string const & providerName = "pva");
    ~PvaMultiChannel();
    /**
     * @brief Destroy the pvAccess connection.
     */
    void destroy();
    /**
     * @brief Get the channelNames.
     * @return The names.
     */
    epics::pvData::PVStringArrayPtr getChannelNames();
    /**
     * @brief Connect to the channel.
     * This calls issueConnect and waitConnect.
     * An exception is thrown if connect fails.
     * @param timeout The time to wait for connecting to the channel.
     * @param maxNotConnected Maximum number of channels that do not connect.
     * @return status of request
     */
    epics::pvData::Status connect(
       double timeout=5,
       size_t maxNotConnected=0);
    /**
     * Are all channels connected?
     * @return if all are connected.
     */
    bool allConnected();
    /**
     * Has a connection state change occured?
     * @return (true, false) if (at least one, no) channel has changed state.
     */
    bool connectionChange();
    /**
     * Get the connection state of each channel.
     * @return The state of each channel.
     */
    epics::pvData::PVBooleanArrayPtr getIsConnected();
    /**
     * Get the pvaChannelArray.
     * @return The weak shared pointer.
     */
    PvaChannelArrayWPtr getPvaChannelArray();
    /**
     * Get pva.
     * @return The weak shared pointer.
     */
    Pva::weak_pointer getPva();
    /**
     * Get the shared pointer to self.
     * @return The shared pointer.
     */
    PvaMultiChannelPtr getPtrSelf()
    {
        return shared_from_this();
    }
private:
    PvaMultiChannel(
        PvaPtr const &pva,
        epics::pvData::PVStringArrayPtr const & channelName,
        std::string const & providerName);

    Pva::weak_pointer pva;
    epics::pvData::PVStringArrayPtr channelName;
    std::string providerName;
    size_t numChannel;
    epics::pvData::Mutex mutex;

    size_t numConnected;
    PvaChannelArrayPtr pvaChannelArray;
    epics::pvData::PVBooleanArrayPtr isConnected;
    bool isDestroyed;
};


}}

#endif  /* PVA_H */

/** @page Overview Documentation
 *
 * <a href = "pvaOverview.html">pvaOverview.html</a>
 *
 */

