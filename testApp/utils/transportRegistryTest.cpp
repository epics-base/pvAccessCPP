/*
 * transportRegistryTest.cpp
 *
 */

#include <pv/transportRegistry.h>
#include <pv/introspectionRegistry.h>
#include <pv/CDRMonitor.h>

#include <epicsAssert.h>
#include <epicsExit.h>
#include <iostream>
#include <string>

namespace epics {
namespace pvAccess {


class TestTransport : public Transport {
public:
    typedef std::tr1::shared_ptr<TestTransport> shared_pointer;
    typedef std::tr1::shared_ptr<const TestTransport> const_shared_pointer;

    TestTransport(string type, int16 priority, osiSockAddr* address): _type(type), _priority(priority), _address(address) {
        /*cout << "Transport::Transport" << endl;*/
    };
    ~TestTransport() {
        /*cout << "Transport::~Transport" << endl;*/
    };
    virtual const string getType() const {
        return _type;
    };
    virtual int16 getPriority() const {
        return _priority;
    };
    virtual const osiSockAddr* getRemoteAddress() const {
        return _address;
    };

    virtual int8 getMajorRevision() const {
        return 0;
    };
    virtual int8 getMinorRevision() const {
        return 0;
    };
    virtual int getReceiveBufferSize() const {
        return 0;
    };
    virtual int getSocketReceiveBufferSize() const {
        return 0;
    };
    virtual void setRemoteMinorRevision(int8 minor) {};
    virtual void setRemoteTransportReceiveBufferSize(
        int receiveBufferSize) {};
    virtual void setRemoteTransportSocketReceiveBufferSize(
        int socketReceiveBufferSize) {};
    virtual void aliveNotification() {};
    virtual void changedTransport() {};
    virtual void close(bool force) {};
    virtual bool isClosed() {
        return false;
    };
    virtual bool isVerified() {
        return false;
    };
    virtual void verified() {};
    virtual void enqueueSendRequest(TransportSender::shared_pointer const & sender) {};
    virtual void ensureData(int) {};
    virtual void alignData(int) {};
    virtual IntrospectionRegistry* getIntrospectionRegistry() {
        return NULL;
    };
private:
    string _type;
    int16 _priority;
    osiSockAddr* _address;
};

}
}

using namespace epics::pvAccess;
using namespace std;

static TransportRegistry* registry;
static const int16 address_max = 10;
static const int16 priority_max = 100;
typedef std::vector<osiSockAddr*> osiSockAddrVector_t;

int main(int argc, char *argv[])
{
    registry = new TransportRegistry();
    auto_ptr<TransportRegistry::transportVector_t> transportArrayOut;
    std::vector<Transport::shared_pointer> transportArrayIn (address_max * priority_max);
    osiSockAddrVector_t addrArray (address_max);
    //address
    for(int32 i = 0; i < address_max; i++)
    {
        osiSockAddr* addr = new osiSockAddr;
        addr->ia.sin_addr.s_addr = i;
        addr->ia.sin_port = i;
        addr->ia.sin_family = AF_INET;
        addrArray.at(i) = addr;

        //priority
        for(int16 j = 0; j < priority_max; j++)
        {
            Transport::shared_pointer testTransportIn(new TestTransport("tcp", j, addr));
            transportArrayIn.at(i * priority_max + j) = testTransportIn;
            registry->put(testTransportIn);

            Transport::shared_pointer testTransportOut(registry->get("tcp",addr,(const int16)j));
            assert(testTransportIn.get() == testTransportOut.get());
        }

        transportArrayOut = registry->get("tcp",addr);
        assert((int16)transportArrayOut->size() == priority_max);
        for(int32 k = 0; k < priority_max; k++)
        {
            assert(transportArrayIn.at(i * priority_max + k).get() == transportArrayOut->at(k).get());
        }
    }

    //add one transport which has same addr and priority as last one and check that the size does not increase
    osiSockAddr* addr = new osiSockAddr;
    addr->ia.sin_addr.s_addr = address_max - 1;
    addr->ia.sin_port = address_max - 1;
    addr->ia.sin_family = AF_INET;
    Transport::shared_pointer testTransportIn(new TestTransport("tcp", priority_max - 1, addr));
    registry->put(testTransportIn);
    Transport::shared_pointer testTransportOut(registry->get("tcp",addr,(const int16)priority_max - 1));
    assert(testTransportIn.get() == testTransportOut.get());
    delete addr;
    //put back the old one
    registry->put(transportArrayIn.at((address_max - 1) * priority_max + priority_max - 1));

    assert(registry->numberOfActiveTransports() == (address_max * priority_max));

    transportArrayOut = registry->toArray("tcp");
    assert((int16)transportArrayOut->size() == (address_max * priority_max));
    for(int32 i = 0; i < address_max * priority_max; i++)
    {
        assert(transportArrayIn.at(i).get() ==  transportArrayOut->at(i).get());
    }


    transportArrayOut = registry->toArray();
    assert((int16)transportArrayOut->size() == (address_max * priority_max));
    for(int32 i = 0; i < address_max * priority_max; i++)
    {
        assert(transportArrayIn.at(i).get() ==  transportArrayOut->at(i).get());
    }

    for(int32 i = 0; i < address_max; i++)
    {
        for(int16 j = 0; j < priority_max; j++)
        {
            assert(transportArrayIn.at(i * priority_max + j) == registry->remove(transportArrayIn.at(i * priority_max + j)));
        }
    }
    assert(registry->numberOfActiveTransports() == 0);

    for(int32 i = 0; i < address_max; i++)
    {
        for(int16 j = 0; j < priority_max; j++)
        {
            registry->put(transportArrayIn.at(i * priority_max + j));
        }
    }
    assert(registry->numberOfActiveTransports() == (priority_max * address_max));
    registry->clear();
    assert(registry->numberOfActiveTransports() == 0);

    for(osiSockAddrVector_t::iterator iter = addrArray.begin(); iter != addrArray.end(); iter++)
    {
        delete *iter;
    }
    addrArray.clear();
    if(registry) delete registry;
    epicsExitCallAtExits();
    epics::pvData::CDRMonitor::get().show(stdout, true);
    return 0;
}


