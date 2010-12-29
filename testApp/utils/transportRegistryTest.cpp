/*
 * transportRegistryTest.cpp
 *
 */

#include "transportRegistry.h"
#include "showConstructDestruct.h"

#include <epicsAssert.h>
#include <iostream>
#include <string>

namespace epics {
    namespace pvAccess {


	class TestTransport : public Transport{
	public:
		TestTransport(string type, int16 priority, osiSockAddr* address): _type(type), _priority(priority), _address(address) {/*cout << "Transport::Transport" << endl;*/};
		~TestTransport(){/*cout << "Transport::~Transport" << endl;*/};
		virtual const string getType() const {return _type;};
		virtual int16 getPriority() const { return _priority;};
		virtual const osiSockAddr* getRemoteAddress() const {return _address;};

		virtual int8 getMajorRevision() const {return 0;};
        virtual int8 getMinorRevision() const {return 0;};
	    virtual int getReceiveBufferSize() const {return 0;};
        virtual int getSocketReceiveBufferSize() const {return 0;};
        virtual void setRemoteMinorRevision(int8 minor) {};
        virtual void setRemoteTransportReceiveBufferSize(
                int receiveBufferSize) {};
        virtual void setRemoteTransportSocketReceiveBufferSize(
                int socketReceiveBufferSize){};
        virtual void aliveNotification(){};
        virtual void changedTransport(){};
        virtual void close(bool force){};
        virtual bool isClosed() const{return false;};
        virtual bool isVerified() const{return false;};
        virtual void verified(){};
        virtual void enqueueSendRequest(TransportSender* sender){};
        virtual void ensureData(int) {};
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

int main(int argc, char *argv[])
{
	registry = new TransportRegistry();
	int32 size;
	TestTransport** transportArrayOut;
	TestTransport** transportArrayIn = new TestTransport*[address_max * priority_max];
	osiSockAddr** addrArray = new osiSockAddr*[address_max];
	//address
	for(int32 i = 0; i < address_max; i++)
	{
		osiSockAddr* addr = new osiSockAddr;
		addrArray[i] = addr;
		addr->ia.sin_addr.s_addr = i;

		//priority
		for(int16 j = 0; j < priority_max; j++)
		{
			TestTransport* transportIn = new TestTransport("tcp", j, addr);
			transportArrayIn[i * priority_max + j] = transportIn;
			registry->put(static_cast<Transport*>(transportIn));

			TestTransport* transportOut = static_cast<TestTransport*>(registry->get("tcp",addr,(const int16)j));
			assert(transportIn == transportOut);
		}


		transportArrayOut = reinterpret_cast<TestTransport**>(registry->get("tcp",addr,size));
		assert(size == priority_max);
		for(int32 k = 0; k < priority_max; k++)
		{
			assert(transportArrayIn[i * priority_max + k] == transportArrayOut[k]);
		}

		delete[] transportArrayOut;
	}

	assert(registry->numberOfActiveTransports() == (address_max * priority_max));

	transportArrayOut = reinterpret_cast<TestTransport**>(registry->toArray("tcp",size));
	assert(size == (address_max * priority_max));
	for(int32 i = 0; i < address_max * priority_max; i++)
	{
		assert(transportArrayIn[i] == transportArrayOut[i]);
	}
	delete[] transportArrayOut;

	transportArrayOut = reinterpret_cast<TestTransport**>(registry->toArray(size));
	assert(size == (address_max * priority_max));
	for(int32 i = 0; i < address_max * priority_max; i++)
	{
		assert(transportArrayIn[i] == transportArrayOut[i]);
	}
	delete[] transportArrayOut;

	for(int32 i = 0; i < address_max; i++)
	{
		for(int16 j = 0; j < priority_max; j++)
		{
			assert(transportArrayIn[i * priority_max + j] == registry->remove(static_cast<Transport*>(transportArrayIn[i * priority_max + j])));
		}
	}
	assert(registry->numberOfActiveTransports() == 0);


	for(int32 i = 0; i < address_max; i++)
	{
		if( addrArray[i]) delete addrArray[i];
		for(int16 j = 0; j < priority_max; j++)
		{
			if(transportArrayIn[i * priority_max + j]) delete transportArrayIn[i * priority_max + j];
		}
	}

	if(addrArray) delete[] addrArray;
	if(transportArrayIn) delete[] transportArrayIn;
	if(registry) delete registry;
	getShowConstructDestruct()->constuctDestructTotals(stdout);
	return 0;
}


