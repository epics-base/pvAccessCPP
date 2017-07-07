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
    std::string name() const;
    void cancel();

protected:
    std::tr1::shared_ptr<Impl> impl;
};

struct epicsShareClass TestPutEvent
{
    enum event_t {
        Fail,
        Cancel,
        Success,
    } event;
    std::string message;
    void *priv;
};

struct epicsShareClass TestGetEvent : public TestPutEvent
{
    epics::pvData::PVStructure::const_shared_pointer value;
};

struct epicsShareClass TestMonitor
{
    struct Impl;
    TestMonitor() {}
    TestMonitor(const std::tr1::shared_ptr<Impl>&);
    ~TestMonitor();

    std::string name() const;
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

struct epicsShareClass TestMonitorEvent
{
    enum event_t {
        Fail=1,      // subscription ends in an error
        Cancel=2,    // subscription ends in cancellation
        Disconnect=4,// subscription interrupted to do lose of communication
        Data=8,      // Data queue not empty
    } event;
    std::string message; // set for event=Fail
    void *priv;
};

struct epicsShareClass TestConnectEvent
{
    bool connected;
};

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

    struct epicsShareClass GetCallback {
        virtual ~GetCallback() {}
        virtual void getDone(const TestGetEvent& evt)=0;
    };

    TestOperation get(GetCallback* cb,
                      epics::pvData::PVStructure::const_shared_pointer pvRequest = epics::pvData::PVStructure::const_shared_pointer());

    TestOperation rpc(GetCallback* cb,
                      const epics::pvData::PVStructure::const_shared_pointer& arguments,
                      epics::pvData::PVStructure::const_shared_pointer pvRequest = epics::pvData::PVStructure::const_shared_pointer());

    struct epicsShareClass PutCallback {
        virtual ~PutCallback() {}
        virtual epics::pvData::PVStructure::const_shared_pointer putBuild(const epics::pvData::StructureConstPtr& build) =0;
        virtual void putDone(const TestPutEvent& evt)=0;
    };

    TestOperation put(PutCallback* cb,
                      epics::pvData::PVStructure::const_shared_pointer pvRequest = epics::pvData::PVStructure::const_shared_pointer());

    struct epicsShareClass MonitorCallback {
        virtual ~MonitorCallback() {}
        virtual void monitorEvent(const TestMonitorEvent& evt)=0;
    };

    TestMonitor monitor(MonitorCallback *cb,
                          epics::pvData::PVStructure::const_shared_pointer pvRequest = epics::pvData::PVStructure::const_shared_pointer());

    struct epicsShareClass ConnectCallback {
        virtual ~ConnectCallback() {}
        virtual void connectEvent(const TestConnectEvent& evt)=0;
    };
    void addConnectListener(ConnectCallback*);
    void removeConnectListener(ConnectCallback*);

private:
    std::tr1::shared_ptr<epics::pvAccess::Channel> getChannel();
};

class epicsShareClass TestClientProvider
{
    std::tr1::shared_ptr<epics::pvAccess::ChannelProvider> provider;
public:

    TestClientProvider(const std::string& providerName,
                       const std::tr1::shared_ptr<epics::pvAccess::Configuration>& conf = std::tr1::shared_ptr<epics::pvAccess::Configuration>());
    ~TestClientProvider();

    TestClientChannel connect(const std::string& name,
                              const TestClientChannel::Options& conf = TestClientChannel::Options());
};

#endif // PVATESTCLIENT_H
