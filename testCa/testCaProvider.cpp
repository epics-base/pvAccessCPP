/* testCaProvider.cpp */
/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
/* Author:  Marty Kraimer Date: 2018.05 */

#include <cstddef>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cstdio>

#include <epicsUnitTest.h>
#include <testMain.h>
#include <epicsVersion.h>
#include <envDefs.h>

#ifdef EPICS_VERSION_INT
  #if EPICS_VERSION_INT >= VERSION_INT(3,16,2,0)
    #define USE_DBUNITTEST
    // USE_TYPED_RSET prevents deprecation warnings
    #define USE_TYPED_RSET
    #define EXIT_TESTS 0
    #include <dbAccess.h>
    #include <errlog.h>
    #include <dbUnitTest.h>

    extern "C" int testIoc_registerRecordDeviceDriver(struct dbBase *pbase);
  #endif
#endif

#ifndef EXIT_TESTS
  #define EXIT_TESTS 1
#endif

#include <pv/thread.h>
#include <pv/pvAccess.h>
#include <pv/convert.h>
#include <pv/caProvider.h>
#include <pv/requester.h>
#include <pv/status.h>
#include <pv/event.h>
#include <pv/lock.h>
#include <pv/pvIntrospect.h>
#include <pv/pvData.h>

// DEBUG must be 0 to run under the automated test harness
#define DEBUG 0

// These need to be longer than you might expect for Jenkins
#define CONNECTION_TIMEOUT 10.0
#define OPERATION_TIMEOUT  10.0

using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::pvAccess::ca;
using namespace std;

class TestChannel;
typedef std::tr1::shared_ptr<TestChannel> TestChannelPtr;

class TestChannel:
    public ChannelRequester,
    public std::tr1::enable_shared_from_this<TestChannel>
{
public:
    POINTER_DEFINITIONS(TestChannel);
    string getRequesterName();
    void message(
        string const & message,
        MessageType messageType);
    virtual void channelCreated(const Status& status, Channel::shared_pointer const & channel);
    virtual void channelStateChange(Channel::shared_pointer const & channel, Channel::ConnectionState connectionState);
    string getChannelName();
    Channel::shared_pointer getChannel();
    static TestChannelPtr create(string const & channelName);
    void connect();
    void waitConnect(double timeout);
private:
    TestChannel(string const & channelName);
    string channelName;
    Event waitForConnect;
    Channel::shared_pointer channel;
};

string TestChannel::getChannelName() { return channelName;}

Channel::shared_pointer TestChannel::getChannel() { return channel;}

TestChannelPtr TestChannel::create(string const & channelName)
{
   TestChannelPtr testChannel(new TestChannel(channelName));
   testChannel->connect();
   return testChannel;
}

TestChannel::TestChannel(string const & channelName)
: channelName(channelName)
{
}

string TestChannel::getRequesterName() { return "testChannel";}
void TestChannel::message(string const & message,MessageType messageType) {};

void TestChannel::channelCreated(const Status& status, Channel::shared_pointer const & channel)
{
    if(channel->isConnected()) waitForConnect.signal();
}

void TestChannel::channelStateChange(Channel::shared_pointer const & channel, Channel::ConnectionState connectionState)
{
    if(connectionState==Channel::CONNECTED) waitForConnect.signal();
}

void TestChannel::connect()
{
    ChannelProviderRegistry::shared_pointer reg(ChannelProviderRegistry::clients());
    ChannelProvider::shared_pointer channelProvider(reg->getProvider("ca"));
    if(!channelProvider) throw std::runtime_error(channelName + " provider ca not registered");
    channel = channelProvider->createChannel(channelName,shared_from_this(),ChannelProvider::PRIORITY_DEFAULT);
    if(!channel) throw std::runtime_error(channelName + " channelCreate failed ");
    waitConnect(CONNECTION_TIMEOUT);
}

void TestChannel::waitConnect(double timeout)
{
    if(waitForConnect.wait(timeout)) return;
    throw std::runtime_error(channelName + " TestChannel::waitConnect failed ");
}

class TestChannelGet;
typedef std::tr1::shared_ptr<TestChannelGet> TestChannelGetPtr;

class TestChannelGetRequester;
typedef std::tr1::shared_ptr<TestChannelGetRequester> TestChannelGetRequesterPtr;
typedef std::tr1::weak_ptr<TestChannelGetRequester> TestChannelGetRequesterWPtr;

class TestChannelGetRequester
{
public:
    virtual void getDone(
        PVStructure::shared_pointer const & pvStructure,
        BitSet::shared_pointer const & bitSet) = 0;
};

class TestChannelGet:
    public ChannelGetRequester,
    public std::tr1::enable_shared_from_this<TestChannelGet>
{
public:
    POINTER_DEFINITIONS(TestChannelGet);
    virtual string getRequesterName();
    virtual void message(string const & message, epics::pvData::MessageType messageType) {}
    virtual void channelGetConnect(
        const Status& status,
        ChannelGet::shared_pointer const & channelGet,
        Structure::const_shared_pointer const & structure);
    virtual void getDone(
        const Status& status,
        ChannelGet::shared_pointer const & channelGet,
        PVStructure::shared_pointer const & pvStructure,
        BitSet::shared_pointer const & bitSet);
    static TestChannelGetPtr create(
        TestChannelGetRequesterPtr const &getRequester,
        TestChannelPtr const &testChannel,
        PVStructurePtr const &  pvRequest);
    void connect();
    void waitConnect(double timeout);
    void get();
private:
    TestChannelGet(
        TestChannelGetRequesterPtr const &getRequester,
        TestChannelPtr const &testChannel,
        PVStructurePtr const &  pvRequest);
    
    TestChannelGetRequesterWPtr getRequester;
    TestChannelPtr testChannel;
    PVStructurePtr pvRequest;
    PVStructurePtr pvStructure;
    Event waitForConnect;
    ChannelGet::shared_pointer channelGet;
};

TestChannelGetPtr TestChannelGet::create(
    TestChannelGetRequesterPtr const &getRequester,
    TestChannelPtr const &testChannel,
    PVStructurePtr const &  pvRequest)
{
    TestChannelGetPtr testChannelGet(new TestChannelGet(getRequester,testChannel,pvRequest));
    testChannelGet->connect();
    testChannelGet->waitConnect(CONNECTION_TIMEOUT);
    return testChannelGet;
}

TestChannelGet::TestChannelGet(
    TestChannelGetRequesterPtr const &getRequester,
    TestChannelPtr const &testChannel,
    PVStructurePtr const &  pvRequest)
    : getRequester(getRequester),
      testChannel(testChannel),
      pvRequest(pvRequest)
{
}

string TestChannelGet::getRequesterName() {return "TestChannelGet";}

void TestChannelGet::channelGetConnect(
    const Status& status,
    ChannelGet::shared_pointer const & channelGet,
    Structure::const_shared_pointer const & structure)
{
    waitForConnect.signal();
}

void TestChannelGet::getDone(
    const Status& status,
    ChannelGet::shared_pointer const & channelGet,
    PVStructure::shared_pointer const & pvStructure,
    BitSet::shared_pointer const & bitSet)
{
    TestChannelGetRequesterPtr req(getRequester.lock());
    if(!req) return;
    if(status.isOK()) {
        req->getDone(pvStructure,bitSet);
        return;
    }
    string message = string("channel ")
      + testChannel->getChannelName()
      + " TestChannelGet::getDone  "
      + status.getMessage();
    throw std::runtime_error(message);
}

void TestChannelGet::connect()
{
    channelGet = testChannel->getChannel()->createChannelGet(shared_from_this(),pvRequest);
    if(!channelGet) throw std::runtime_error(testChannel->getChannelName() + " channelCreate failed ");
}

void TestChannelGet::waitConnect(double timeout)
{
    if(waitForConnect.wait(timeout)) return;
    throw std::runtime_error(testChannel->getChannelName() + " TestChannelGet::waitConnect failed ");
}


void TestChannelGet::get()
{
    channelGet->get();
}

class TestChannelPut;
typedef std::tr1::shared_ptr<TestChannelPut> TestChannelPutPtr;

class TestChannelPutRequester;
typedef std::tr1::shared_ptr<TestChannelPutRequester> TestChannelPutRequesterPtr;
typedef std::tr1::weak_ptr<TestChannelPutRequester> TestChannelPutRequesterWPtr;

class TestChannelPutRequester
{
public:
    virtual void putDone() = 0;
};

class TestChannelPut:
    public ChannelPutRequester,
    public std::tr1::enable_shared_from_this<TestChannelPut>
{
public:
    POINTER_DEFINITIONS(TestChannelPut);
    virtual string getRequesterName();
    virtual void message(string const & message, MessageType messageType) {}
    virtual void channelPutConnect(
        const Status& status,
        ChannelPut::shared_pointer const & channelPut,
        Structure::const_shared_pointer const & structure);
    virtual void putDone(
        const Status& status,
        ChannelPut::shared_pointer const & channelPut);
    virtual void getDone(
        const Status& status,
        ChannelPut::shared_pointer const & channelPut,
        PVStructure::shared_pointer const & pvStructure,
        BitSet::shared_pointer const & bitSet);
    static TestChannelPutPtr create(
        TestChannelPutRequesterPtr const &putRequester,
        TestChannelPtr const &testChannel);
    void connect();
    void waitConnect(double timeout);
    void put(string const & value);
private:
    TestChannelPut(
        TestChannelPutRequesterPtr const &putRequester,
        TestChannelPtr const &testChannel);
    
    TestChannelPutRequesterWPtr putRequester;
    TestChannelPtr testChannel;
    PVStructurePtr pvStructure;
    BitSetPtr bitSet;
    Event waitForConnect;
    ChannelPut::shared_pointer channelPut;
};

TestChannelPutPtr TestChannelPut::create(
    TestChannelPutRequesterPtr const &putRequester,
    TestChannelPtr const &testChannel)
{
    TestChannelPutPtr testChannelPut(new TestChannelPut(putRequester,testChannel));
    testChannelPut->connect();
    testChannelPut->waitConnect(CONNECTION_TIMEOUT);
    return testChannelPut;
}

TestChannelPut::TestChannelPut(
    TestChannelPutRequesterPtr const &putRequester,
    TestChannelPtr const &testChannel)
    : putRequester(putRequester),
      testChannel(testChannel)
{
}

string TestChannelPut::getRequesterName() {return "TestChannelPut";}

void TestChannelPut::channelPutConnect(
    const Status& status,
    ChannelPut::shared_pointer const & channelPut,
    Structure::const_shared_pointer const & structure)
{
    pvStructure = PVDataCreate::getPVDataCreate()->createPVStructure(structure);
    bitSet = BitSetPtr(new BitSet(pvStructure->getNumberFields()));
    waitForConnect.signal();
}

void TestChannelPut::getDone(
    const Status& status,
    ChannelPut::shared_pointer const & channelPut,
    PVStructure::shared_pointer const & pvStructure,
    BitSet::shared_pointer const & bitSet)
{
    throw std::runtime_error("TestChannelPut::getDone should not be called");
}

void TestChannelPut::putDone(
        const Status& status,
        ChannelPut::shared_pointer const & channelPut)
{
    TestChannelPutRequesterPtr req(putRequester.lock());
    if(!req) return;
    if(status.isOK()) {
        req->putDone();
        return;
    }
    string message = string("channel ")
      + testChannel->getChannelName()
      + " TestChannelPut::putDone  "
      + status.getMessage();
    throw std::runtime_error(message);
}

void TestChannelPut::connect()
{
    string request("value");
    PVStructurePtr pvRequest(createRequest(request));
    
    channelPut = testChannel->getChannel()->createChannelPut(shared_from_this(),pvRequest);
    if(!channelPut) throw std::runtime_error(testChannel->getChannelName() + " channelCreate failed ");
}

void TestChannelPut::waitConnect(double timeout)
{
    if(waitForConnect.wait(timeout)) return;
    throw std::runtime_error(testChannel->getChannelName() 
        + " TestChannelPut::waitConnect failed ");
}


void TestChannelPut::put(string const & value)
{
    PVFieldPtr pvField(pvStructure->getSubField("value"));
    if(!pvField) throw std::runtime_error(testChannel->getChannelName() 
         + " TestChannelPut::put no value ");
    FieldConstPtr field(pvField->getField());
    Type type(field->getType());
    if(type==scalar) {
        PVScalarPtr pvScalar(std::tr1::static_pointer_cast<PVScalar>(pvField));
        getConvert()->fromString(pvScalar,value);
        bitSet->set(pvField->getFieldOffset());
        channelPut->put(pvStructure,bitSet);
        return;
    }
    if(type==scalarArray) {
        PVScalarArrayPtr pvScalarArray(std::tr1::static_pointer_cast<PVScalarArray>(pvField));
        std::vector<string> values;
        size_t pos = 0;
        size_t n = 1;
        while(true)
        {
            size_t offset = value.find(" ",pos);
            if(offset==string::npos) {
                values.push_back(value.substr(pos));
                break;
            }
            values.push_back(value.substr(pos,offset-pos));
            pos = offset+1;
            n++;    
        }
        pvScalarArray->setLength(n);
        getConvert()->fromStringArray(pvScalarArray,0,n,values,0);       
        bitSet->set(pvField->getFieldOffset());
        channelPut->put(pvStructure,bitSet);
        return;
    }
    if(type==structure) {
       PVScalarPtr pvScalar(pvStructure->getSubField<PVScalar>("value.index"));
       if(pvScalar) {
          getConvert()->fromString(pvScalar,value);
          bitSet->set(pvScalar->getFieldOffset());
          channelPut->put(pvStructure,bitSet);
          return;
       }
    }
    throw std::runtime_error(testChannel->getChannelName() 
        + " TestChannelPut::put not supported  type");
}

class TestChannelMonitor;
typedef std::tr1::shared_ptr<TestChannelMonitor> TestChannelMonitorPtr;

class TestChannelMonitorRequester;
typedef std::tr1::shared_ptr<TestChannelMonitorRequester> TestChannelMonitorRequesterPtr;
typedef std::tr1::weak_ptr<TestChannelMonitorRequester> TestChannelMonitorRequesterWPtr;

class TestChannelMonitorRequester
{
public:
    virtual void monitorEvent(
        PVStructure::shared_pointer const & pvStructure,
        BitSet::shared_pointer const & bitSet) = 0;
};

class TestChannelMonitor:
    public MonitorRequester,
    public std::tr1::enable_shared_from_this<TestChannelMonitor>
{
public:
    POINTER_DEFINITIONS(TestChannelMonitor);
    virtual string getRequesterName();
    virtual void message(string const & message, MessageType messageType) {}
    virtual void monitorConnect(
        Status const & status,
        MonitorPtr const & monitor,
        StructureConstPtr const & structure);
    virtual void monitorEvent(MonitorPtr const & monitor);
    virtual void unlisten(MonitorPtr const & monitor);
    static TestChannelMonitorPtr create(
        TestChannelMonitorRequesterPtr const &putRequester,
        TestChannelPtr const &testChannel,
        PVStructurePtr const &  pvRequest);
    void connect();
    void waitConnect(double timeout);
    void stopEvents();
private:
    TestChannelMonitor(
        TestChannelMonitorRequesterPtr const &putRequester,
        TestChannelPtr const &testChannel,
        PVStructurePtr const &  pvRequest);
    
    TestChannelMonitorRequesterWPtr monitorRequester;
    TestChannelPtr testChannel;
    PVStructurePtr pvRequest;
    Event waitForConnect;
    Monitor::shared_pointer channelMonitor;
};

TestChannelMonitorPtr TestChannelMonitor::create(
    TestChannelMonitorRequesterPtr const &monitorRequester,
    TestChannelPtr const &testChannel,
    PVStructurePtr const &  pvRequest)
{
    TestChannelMonitorPtr testChannelMonitor(new TestChannelMonitor(monitorRequester,testChannel,pvRequest));
    testChannelMonitor->connect();
    testChannelMonitor->waitConnect(CONNECTION_TIMEOUT);
    return testChannelMonitor;
}

TestChannelMonitor::TestChannelMonitor(
    TestChannelMonitorRequesterPtr const &monitorRequester,
    TestChannelPtr const &testChannel,
    PVStructurePtr const &  pvRequest)
    : monitorRequester(monitorRequester),
      testChannel(testChannel),
      pvRequest(pvRequest)
{
}

string TestChannelMonitor::getRequesterName() {return "TestChannelMonitor";}

void TestChannelMonitor::monitorConnect(
    Status const & status,
    MonitorPtr const & monitor,
    StructureConstPtr const & structure)
{
    waitForConnect.signal();
}
    

void TestChannelMonitor::monitorEvent(MonitorPtr const & monitor)
{
    TestChannelMonitorRequesterPtr req(monitorRequester.lock());
    if(!req) return;
    while(true) {
        MonitorElementPtr monitorElement = monitor->poll();
        if(!monitorElement) return;
        req->monitorEvent(monitorElement->pvStructurePtr,monitorElement->changedBitSet);
        monitor->release(monitorElement);
    }
}


void TestChannelMonitor::unlisten(MonitorPtr const & monitor)
{
}

void TestChannelMonitor::connect()
{
    channelMonitor = testChannel->getChannel()->createMonitor(shared_from_this(),pvRequest);
    if(!channelMonitor) throw std::runtime_error(testChannel->getChannelName()
         + " TestChannelMonitor::connect failed ");
}

void TestChannelMonitor::waitConnect(double timeout)
{
    if(waitForConnect.wait(timeout)) {
         channelMonitor->start();
         return;
    }
    throw std::runtime_error(testChannel->getChannelName()
       + " TestChannelMonitor::waitConnect failed ");
}

void TestChannelMonitor::stopEvents()
{
    channelMonitor->stop();
}

class TestClient;
typedef std::tr1::shared_ptr<TestClient> TestClientPtr;

class TestClient:
    public TestChannelGetRequester,
    public TestChannelPutRequester,
    public TestChannelMonitorRequester,
    public std::tr1::enable_shared_from_this<TestClient>
{
public:
    POINTER_DEFINITIONS(TestClient);
    virtual void getDone(
        PVStructure::shared_pointer const & pvStructure,
        BitSet::shared_pointer const & bitSet);
   virtual void putDone();
   virtual void monitorEvent(
        PVStructure::shared_pointer const & pvStructure,
        BitSet::shared_pointer const & bitSet);
   static TestClientPtr create(string const &channelName,PVStructurePtr const &  pvRequest);
   void connect();
   void get();
   void put(string const & value);
   void waitGet(double timeout);
   void waitPut(double timeout);
   void stopEvents();
private:
   TestClient(string const &channelName,PVStructurePtr const &  pvRequest);
   string channelName;
   PVStructurePtr pvRequest;
   string putValue;
   TestChannelPtr testChannel;
   TestChannelGetPtr testChannelGet;
   TestChannelPutPtr testChannelPut;
   TestChannelMonitorPtr testChannelMonitor;
   Event waitForGet;
   Event waitForPut;
};

TestClientPtr TestClient::create(string const &channelName,PVStructurePtr const &  pvRequest)
{
    TestClientPtr testClient(new TestClient(channelName,pvRequest));
    testClient->connect();
    return testClient;
}

TestClient::TestClient(string const &channelName,PVStructurePtr const &  pvRequest)
   : channelName(channelName),
     pvRequest(pvRequest)
{
}

void TestClient::connect()
{
    testChannel = TestChannel::create(channelName);
    testChannelGet = TestChannelGet::create(shared_from_this(),testChannel,pvRequest);
    testChannelPut = TestChannelPut::create(shared_from_this(),testChannel);
    testChannelMonitor = TestChannelMonitor::create(shared_from_this(),testChannel,pvRequest);
}

void TestClient::getDone(
    PVStructure::shared_pointer const & pvStructure,
    BitSet::shared_pointer const & bitSet)
{
   testOk(pvStructure.get() != 0,"pvStructure not null");
   testOk(pvStructure->getSubField("value").get() != 0,"value not null");
   testOk(pvStructure->getSubField("timeStamp").get() != 0,"timeStamp not null");
   testOk(pvStructure->getSubField("alarm").get() != 0,"alarm not null");
   if (DEBUG) std::cout << testChannel->getChannelName() + " TestClient::getDone"
             << " putValue " << putValue
             << " bitSet " << *bitSet
             << " pvStructure\n" << pvStructure << "\n";
   PVScalarPtr pvScalar = pvStructure->getSubField<PVScalar>("value");
   if(pvScalar) {
        string getValue = getConvert()->toString(pvScalar);
        testOk(getValue.compare(putValue)==0,"getValue==putValue");
   }
   waitForGet.signal();
}

void TestClient::putDone()
{
    waitForPut.signal();
}

void TestClient::monitorEvent(
        PVStructure::shared_pointer const & pvStructure,
        BitSet::shared_pointer const & bitSet)
{
   if (DEBUG) std::cout << testChannel->getChannelName() + " TestClient::monitorEvent"
             << " bitSet " << *bitSet
             << " pvStructure\n" << pvStructure << "\n";
}

void TestClient::get()
{
    testDiag("TestClient::get %s",
        testChannel->getChannelName().c_str());
    testChannelGet->get();
   if (DEBUG) cout << "TestClient::get() calling waitGet\n";
    waitGet(OPERATION_TIMEOUT);
}

void TestClient::waitGet(double timeout)
{
    testOk(waitForGet.wait(timeout),
        "waitGet(%s) succeeded", testChannel->getChannelName().c_str());
}

void TestClient::put(string const & value)
{
    testDiag("TestClient::put %s := %s",
        testChannel->getChannelName().c_str(), value.c_str());
    putValue = value;
    testChannelPut->put(value);
    waitPut(OPERATION_TIMEOUT);
}

void TestClient::waitPut(double timeout)
{
    testOk(waitForPut.wait(timeout),
        "waitPut(%s) succeeded", testChannel->getChannelName().c_str());
}

void TestClient::stopEvents()
{
    testChannelMonitor->stopEvents();
}

class TestIoc;
typedef std::tr1::shared_ptr<TestIoc> TestIocPtr;

class TestIoc :
    public epicsThreadRunable
{
public:
    virtual void run();
    static TestIocPtr create();
    void start();
    void shutdown();
private:
#ifndef USE_DBUNITTEST
    std::auto_ptr<epicsThread> thread;
    const char *base;
    const char *arch;
#endif
};

TestIocPtr TestIoc::create()
{
    return TestIocPtr(new TestIoc());
}

void TestIoc::start()
{
#ifdef USE_DBUNITTEST
    testdbPrepare();
    testdbReadDatabase("testIoc.dbd", NULL, NULL);
    testIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("testCaProvider.db", NULL, NULL);
    eltc(0);
    testIocInitOk();
    eltc(1);
#else
    base = getenv("EPICS_BASE");
    if (!base)
        testAbort("Environment variable $EPICS_BASE not defined");
    arch = getenv("EPICS_HOST_ARCH");
    if (!arch)
        testAbort("Environment variable $EPICS_HOST_ARCH not defined");
    epicsEnvSet("EPICS_CA_ADDR_LIST", "localhost");
    epicsEnvSet("EPICS_CA_AUTO_ADDR_LIST", "NO");

    thread =  std::auto_ptr<epicsThread>(new epicsThread(
        *this,
        "testIoc",
        epicsThreadGetStackSize(epicsThreadStackBig),
        epicsThreadPriorityLow));
    thread->start();  
#endif
}

void TestIoc::run()
{
#ifndef USE_DBUNITTEST
    // Base-3.14 doesn't provide the dbUnitTest APIs, and the CA
    // tests with an embedded IOC fail with a Base before 3.16.2.
    // This version only works on workstation targets, it runs the
    // softIoc from Base as a separate process, using system().
    if(system("$EPICS_BASE/bin/$EPICS_HOST_ARCH/softIoc -x test -d ../testCaProvider.db")!=0) {
        string message(base);
        message += "/bin/";
        message += arch;
        message += "/softIoc -d ../testCaProvider.db not started";
        throw std::runtime_error(message);
    }
#endif
}

void TestIoc::shutdown()
{
#ifdef USE_DBUNITTEST
    testIocShutdownOk();
    testdbCleanup();
#endif
}

void checkClient(const string &channelName, const string &putValue)
{
    string request("value,alarm,timeStamp");
    PVStructurePtr pvRequest(createRequest(request));
    TestClientPtr client = TestClient::create(channelName,pvRequest);
    if (!client)
        testAbort("NULL client for %s", channelName.c_str());
    client->put(putValue);
    client->get();
    client->stopEvents();
}

MAIN(testCaProvider)
{
    testPlan(143 + EXIT_TESTS);

    TestIocPtr testIoc(new TestIoc());
    testIoc->start();  

    testDiag("===Test caProvider===");
    CAClientFactory::start();
    ChannelProviderRegistry::shared_pointer reg(ChannelProviderRegistry::clients());
    try {
        ChannelProvider::shared_pointer channelProvider(reg->getProvider("ca"));
        if (!channelProvider) testAbort("Channel provider 'ca' not registered");
        checkClient("DBRlongout", "0");
        checkClient("DBRlongout", "1");
        checkClient("DBRlongout", "-1");
        checkClient("DBRlongout", "32767");
        checkClient("DBRlongout", "32768");
        checkClient("DBRlongout", "-32768");
        checkClient("DBRlongout", "-32769");
        checkClient("DBRlongout", "2147483647");
        checkClient("DBRlongout", "-2147483648");
        checkClient("DBRdoubleout", "1.5");
        checkClient("DBRstringout", "test");
        checkClient("DBRbyteArray", "1 2 3");
        checkClient("DBRshortArray", "1 2 3");
        checkClient("DBRintArray", "1 2 3");
        checkClient("DBRubyteArray", "1 2 3");
        checkClient("DBRushortArray", "1 2 3");
        checkClient("DBRuintArray", "1 2 3");
        checkClient("DBRfloatArray", "1 2 3");
        checkClient("DBRdoubleArray", "1 2 3");
        checkClient("DBRstringArray", "aa bb cc");
        checkClient("DBRmbbout", "2");
        checkClient("DBRbinaryout", "1");

#ifndef USE_DBUNITTEST
        // put to record that makes IOC exit
        string channelName = "test:exit";
        string request("value");
        PVStructurePtr pvRequest(createRequest(request));
        TestClientPtr client = TestClient::create(channelName,pvRequest);
        if (!client)
            testAbort("NULL client for %s", channelName.c_str());
        client->put("1");
        client->stopEvents();
#endif
    }
    catch (std::exception& e) {
        testAbort("caught un-expected exception: %s", e.what());
    }

    testIoc->shutdown();

    return testDone();;
}

