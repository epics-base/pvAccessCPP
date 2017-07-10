/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
#ifndef PVATESTCLIENT_H
#define PVATESTCLIENT_H

#include <epicsMutex.h>

#include <pv/pvData.h>
#include <pv/bitSet.h>

namespace epics {namespace pvAccess {
class ChannelProvider;
class Channel;
class Monitor;
class Configuration;
}}//namespace epics::pvAccess

//! Information on get/put/rpc completion
struct epicsShareClass TestOperation
{
    struct Impl
    {
        virtual ~Impl() {}
        virtual std::string name() const =0;
        virtual void cancel() =0;
    };

    TestOperation() {}
    TestOperation(const std::tr1::shared_ptr<Impl>&);
    ~TestOperation();
    //! Channel name
    std::string name() const;
    //! Immediate cancellation.
    //! Does not wait for remote confirmation.
    void cancel();

protected:
    std::tr1::shared_ptr<Impl> impl;
};

//! Information on put completion
struct epicsShareClass TestPutEvent
{
    enum event_t {
        Fail,    //!< request ends in failure.  Check message
        Cancel,  //!< request cancelled before completion
        Success, //!< It worked!
    } event;
    std::string message;
    void *priv;
};

//! Information on get/rpc completion
struct epicsShareClass TestGetEvent : public TestPutEvent
{
    //! New data. NULL unless event==Success
    epics::pvData::PVStructure::const_shared_pointer value;
};

//! Handle for monitor subscription
struct epicsShareClass TestMonitor
{
    struct Impl;
    TestMonitor() {}
    TestMonitor(const std::tr1::shared_ptr<Impl>&);
    ~TestMonitor();

    //! Channel name
    std::string name() const;
    //! Immediate cancellation.
    //! Does not wait for remote confirmation.
    void cancel();
    //! updates root, changed, overrun
    //! return true if root!=NULL
    bool poll();
    //! true if all events received.
    bool complete() const;
    epics::pvData::PVStructure::const_shared_pointer root;
    epics::pvData::BitSet changed,
                          overrun;

protected:
    std::tr1::shared_ptr<Impl> impl;
};

//! Information on monitor subscription/queue change
struct epicsShareClass TestMonitorEvent
{
    enum event_t {
        Fail=1,      //!< subscription ends in an error
        Cancel=2,    //!< subscription ends in cancellation
        Disconnect=4,//!< subscription interrupted to do lose of communication
        Data=8,      //!< Data queue not empty.  Call TestMonitor::poll()
    } event;
    std::string message; // set for event=Fail
    void *priv;
};

//! informaiton on connect/disconnect
struct epicsShareClass TestConnectEvent
{
    bool connected;
};

//! Represents a single channel
class epicsShareClass TestClientChannel
{
    struct Impl;
    std::tr1::shared_ptr<Impl> impl;
public:
    struct Options {
        short priority;
        std::string address;
        Options();
    };

    TestClientChannel() {}
    TestClientChannel(const std::tr1::shared_ptr<epics::pvAccess::ChannelProvider>& provider,
                      const std::string& name,
                      const Options& opt = Options());
    ~TestClientChannel();

    //! callback for get() and rpc()
    struct epicsShareClass GetCallback {
        virtual ~GetCallback() {}
        virtual void getDone(const TestGetEvent& evt)=0;
    };

    //! Issue request to retrieve current PV value
    //! @param cb Completion notification callback.  Must outlive TestOperation (call TestOperation::cancel() to force release)
    TestOperation get(GetCallback* cb,
                      epics::pvData::PVStructure::const_shared_pointer pvRequest = epics::pvData::PVStructure::const_shared_pointer());

    //! Start an RPC call
    //! @param cb Completion notification callback.  Must outlive TestOperation (call TestOperation::cancel() to force release)
    TestOperation rpc(GetCallback* cb,
                      const epics::pvData::PVStructure::const_shared_pointer& arguments,
                      epics::pvData::PVStructure::const_shared_pointer pvRequest = epics::pvData::PVStructure::const_shared_pointer());

    //! callbacks for put()
    struct epicsShareClass PutCallback {
        virtual ~PutCallback() {}
        //! Called to build the value to be sent once the type info is known
        virtual epics::pvData::PVStructure::const_shared_pointer putBuild(const epics::pvData::StructureConstPtr& build) =0;
        virtual void putDone(const TestPutEvent& evt)=0;
    };

    //! Initiate request to change PV
    //! @param cb Completion notification callback.  Must outlive TestOperation (call TestOperation::cancel() to force release)
    //! TODO: produce bitset to mask fields being set
    TestOperation put(PutCallback* cb,
                      epics::pvData::PVStructure::const_shared_pointer pvRequest = epics::pvData::PVStructure::const_shared_pointer());

    struct epicsShareClass MonitorCallback {
        virtual ~MonitorCallback() {}
        virtual void monitorEvent(const TestMonitorEvent& evt)=0;
    };

    //! Begin subscription
    //! @param cb Completion notification callback.  Must outlive TestOperation (call TestOperation::cancel() to force release)
    TestMonitor monitor(MonitorCallback *cb,
                          epics::pvData::PVStructure::const_shared_pointer pvRequest = epics::pvData::PVStructure::const_shared_pointer());

    //! Connection state change CB
    struct epicsShareClass ConnectCallback {
        virtual ~ConnectCallback() {}
        virtual void connectEvent(const TestConnectEvent& evt)=0;
    };
    //! Append to list of listeners
    //! @param cb Channel dis/connect notification callback.  Must outlive TestClientChannel or call to removeConnectListener()
    void addConnectListener(ConnectCallback*);
    //! Remove from list of listeners
    void removeConnectListener(ConnectCallback*);

private:
    std::tr1::shared_ptr<epics::pvAccess::Channel> getChannel();
};

//! Central client context.
class epicsShareClass TestClientProvider
{
    std::tr1::shared_ptr<epics::pvAccess::ChannelProvider> provider;
public:

    TestClientProvider(const std::string& providerName,
                       const std::tr1::shared_ptr<epics::pvAccess::Configuration>& conf = std::tr1::shared_ptr<epics::pvAccess::Configuration>());
    ~TestClientProvider();

    //! Get a new Channel
    //! Does not block.
    TestClientChannel connect(const std::string& name,
                              const TestClientChannel::Options& conf = TestClientChannel::Options());
};

#endif // PVATESTCLIENT_H
