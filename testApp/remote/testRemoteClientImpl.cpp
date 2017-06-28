
/* testRemoteClientImpl.cpp */
/* Author:  Matej Sekoranja Date: 2011.1.1 */


#include <iostream>
#include <sstream>
#include <epicsExit.h>
#include <pv/clientContextImpl.h>
#include <pv/clientFactory.h>

using namespace epics::pvData;
using namespace epics::pvAccess;
using std::string;

#define SLEEP_TIME 1.0

class ChannelFindRequesterImpl : public ChannelFindRequester
{
    virtual void channelFindResult(const epics::pvData::Status& status,
                                   ChannelFind::shared_pointer const & /*channelFind*/,
                                   bool wasFound)
    {
        std::cout << "[ChannelFindRequesterImpl] channelFindResult("
                  << status << ", ..., " << wasFound << ")" << std::endl;
    }
};

class ChannelRequesterImpl : public ChannelRequester
{
    virtual string getRequesterName()
    {
        return "ChannelRequesterImpl";
    };

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelCreated(const epics::pvData::Status& status, Channel::shared_pointer const & channel)
    {
        std::cout << "channelCreated(" << status << ", "
                  << (channel ? channel->getChannelName() : "(0)") << ")" << std::endl;
    }

    virtual void channelStateChange(Channel::shared_pointer const & c, Channel::ConnectionState connectionState)
    {
        std::cout << "channelStateChange(" << c->getChannelName() << ", " << Channel::ConnectionStateNames[connectionState] << ")" << std::endl;
    }
};

class GetFieldRequesterImpl : public GetFieldRequester
{
    virtual string getRequesterName()
    {
        return "GetFieldRequesterImpl";
    };

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void getDone(const epics::pvData::Status& status, epics::pvData::FieldConstPtr const & field)
    {
        std::cout << "getDone(" << status << ", ";
        if (status.isSuccess() && field)
        {
            std::cout << *field;
        }
        else
            std::cout << "(0)";
        std::cout << ")" << std::endl;
    }
};

class ChannelGetRequesterImpl : public ChannelGetRequester
{
public:

    virtual string getRequesterName()
    {
        return "ChannelGetRequesterImpl";
    };

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelGetConnect(const epics::pvData::Status& status,ChannelGet::shared_pointer const & /*channelGet*/,
                                   epics::pvData::Structure::const_shared_pointer const & pvStructure)
    {
        std::cout << "channelGetConnect(" << status << ")" << std::endl;
        if (status.isSuccess())
        {
            std::cout << *pvStructure << std::endl;
        }
    }

    virtual void getDone(const epics::pvData::Status& status, ChannelGet::shared_pointer const &,
                         PVStructure::shared_pointer const & getData, BitSet::shared_pointer const & /*bitSet*/)
    {
        std::cout << "getDone(" << status << ")" << std::endl;

        if (status.isSuccess())
        {
            std::cout << *getData << std::endl;
        }
    }
};

class ChannelPutRequesterImpl : public ChannelPutRequester
{
public:

    virtual string getRequesterName()
    {
        return "ChannelPutRequesterImpl";
    };

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelPutConnect(const epics::pvData::Status& status,ChannelPut::shared_pointer const & /*channelPut*/,
                                   epics::pvData::Structure::const_shared_pointer const & pvStructure)
    {
        std::cout << "channelPutConnect(" << status << ")" << std::endl;
        if (status.isSuccess())
        {
            std::cout << *pvStructure << std::endl;
        }
    }

    virtual void getDone(const epics::pvData::Status& status, ChannelPut::shared_pointer const &,
                         PVStructure::shared_pointer const & getData, BitSet::shared_pointer const & /*bitSet*/)
    {
        std::cout << "getDone(" << status << ")" << std::endl;
        if (status.isSuccess())
        {
            std::cout << *getData << std::endl;
        }
    }

    virtual void putDone(const epics::pvData::Status& status, ChannelPut::shared_pointer const &)
    {
        std::cout << "putDone(" << status << ")" << std::endl;
    }

};

class ChannelPutGetRequesterImpl : public ChannelPutGetRequester
{
public:

    virtual string getRequesterName()
    {
        return "ChannelGetPutRequesterImpl";
    };

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelPutGetConnect(const epics::pvData::Status& status, ChannelPutGet::shared_pointer const & /*channelPutGet*/,
                                      epics::pvData::Structure::const_shared_pointer const & putData,
                                      epics::pvData::Structure::const_shared_pointer const & getData)
    {
        std::cout << "channelGetPutConnect(" << status << ")" << std::endl;

        if (putData)
        {
            std::cout << *putData << std::endl;
        }
        if (getData)
        {
            std::cout << *getData << std::endl;
        }
    }

    virtual void getGetDone(const epics::pvData::Status& status, ChannelPutGet::shared_pointer const &,
                            PVStructure::shared_pointer const & getData, BitSet::shared_pointer const & /*bitSet*/)
    {
        std::cout << "getGetDone(" << status << ")" << std::endl;
        if (status.isSuccess())
        {
            std::cout << *getData << std::endl;
        }
    }

    virtual void getPutDone(const epics::pvData::Status& status, ChannelPutGet::shared_pointer const &,
                            PVStructure::shared_pointer const & putData, BitSet::shared_pointer const & /*bitSet*/)
    {
        std::cout << "getPutDone(" << status << ")" << std::endl;
        if (status.isSuccess())
        {
            std::cout << *putData << std::endl;
        }
    }

    virtual void putGetDone(const epics::pvData::Status& status, ChannelPutGet::shared_pointer const &,
                            PVStructure::shared_pointer const & putData, BitSet::shared_pointer const & /*bitSet*/)
    {
        std::cout << "putGetDone(" << status << ")" << std::endl;
        if (status.isSuccess())
        {
            std::cout << *putData << std::endl;
        }
    }

};


class ChannelRPCRequesterImpl : public ChannelRPCRequester
{
    virtual string getRequesterName()
    {
        return "ChannelRPCRequesterImpl";
    };

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelRPCConnect(const epics::pvData::Status& status,
                                   ChannelRPC::shared_pointer const & /*channelRPC*/)
    {
        std::cout << "channelRPCConnect(" << status << ")" << std::endl;
    }

    virtual void requestDone(const epics::pvData::Status& status, ChannelRPC::shared_pointer const &,
                             epics::pvData::PVStructure::shared_pointer const & pvResponse)
    {
        std::cout << "requestDone(" << status << ")" << std::endl;
        if (status.isSuccess())
        {
            std::cout << *pvResponse << std::endl;
        }
    }
};

class ChannelArrayRequesterImpl : public ChannelArrayRequester
{
public:

    virtual string getRequesterName()
    {
        return "ChannelArrayRequesterImpl";
    };

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelArrayConnect(const epics::pvData::Status& status,ChannelArray::shared_pointer const & /*channelArray*/,
                                     epics::pvData::Array::const_shared_pointer const & array)
    {
        std::cout << "channelArrayConnect(" << status << ")" << std::endl;
        if (status.isSuccess())
        {
            std::cout << array << std::endl;
        }

    }

    virtual void getArrayDone(const epics::pvData::Status& status, ChannelArray::shared_pointer const &,
                              PVArray::shared_pointer const & pvArray)
    {
        std::cout << "getArrayDone(" << status << ")" << std::endl;
        if (status.isSuccess())
        {
            std::cout << *pvArray << std::endl;
        }
    }

    virtual void putArrayDone(const epics::pvData::Status& status, ChannelArray::shared_pointer const &)
    {
        std::cout << "putArrayDone(" << status << ")" << std::endl;
    }

    virtual void setLengthDone(const epics::pvData::Status& status, ChannelArray::shared_pointer const &)
    {
        std::cout << "setLengthDone(" << status << ")" << std::endl;
    }

    virtual void getLengthDone(const epics::pvData::Status& status, ChannelArray::shared_pointer const &,
                               size_t length)
    {
        std::cout << "getLengthDone(" << status << "," << length << ")" << std::endl;
    }
};

class MonitorRequesterImpl : public MonitorRequester
{
    virtual string getRequesterName()
    {
        return "MonitorRequesterImpl";
    };

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void monitorConnect(const epics::pvData::Status& status,
                                Monitor::shared_pointer const & /*monitor*/, StructureConstPtr const & structure)
    {
        std::cout << "monitorConnect(" << status << ")" << std::endl;
        if (status.isSuccess() && structure)
        {
            std::cout << *structure << std::endl;
        }
    }

    virtual void monitorEvent(Monitor::shared_pointer const & monitor)
    {
        std::cout << "monitorEvent" << std::endl;

        MonitorElement::shared_pointer element = monitor->poll();

        std::cout << "changed/overrun " << *element->changedBitSet << '/' <<
                  *element->overrunBitSet << std::endl << *element->pvStructurePtr << std::endl;

        monitor->release(element);
    }

    virtual void unlisten(Monitor::shared_pointer const & /*monitor*/)
    {
        std::cout << "unlisten" << std::endl;
    }
};


class ChannelProcessRequesterImpl : public ChannelProcessRequester
{
    //ChannelProcess::shared_pointer const & m_channelProcess;

    virtual string getRequesterName()
    {
        return "ProcessRequesterImpl";
    };

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelProcessConnect(const epics::pvData::Status& status,
                                       ChannelProcess::shared_pointer const & /*channelProcess*/)
    {
        std::cout << "channelProcessConnect(" << status << ")" << std::endl;

        //m_channelProcess = channelProcess;
    }

    virtual void processDone(const epics::pvData::Status& status, ChannelProcess::shared_pointer const &)
    {
        std::cout << "processDone(" << status << ")" << std::endl;
    }

};


int main()
{
    for (int i = 0; i < 10; i++) {
        {

            ClientFactory::start();
            ChannelProvider::shared_pointer provider = ChannelProviderRegistry::clients()->getProvider("pva");

            ChannelFindRequester::shared_pointer findRequester(new ChannelFindRequesterImpl());
            ChannelFind::shared_pointer channelFind = provider->channelFind("testSomething", findRequester);
            epicsThreadSleep ( SLEEP_TIME );
            //channelFind->destroy();

            ChannelRequester::shared_pointer channelRequester(new ChannelRequesterImpl());
            Channel::shared_pointer channel = provider->createChannel("testStructureArrayTest", channelRequester);
            epicsThreadSleep ( SLEEP_TIME );
            channel->printInfo();

            {
                GetFieldRequester::shared_pointer getFieldRequesterImpl(new GetFieldRequesterImpl());
                channel->getField(getFieldRequesterImpl, "");
                epicsThreadSleep ( SLEEP_TIME );
            }

            {
                ChannelProcessRequester::shared_pointer channelProcessRequester(new ChannelProcessRequesterImpl());
                PVStructure::shared_pointer pvRequest;
                ChannelProcess::shared_pointer channelProcess = channel->createChannelProcess(channelProcessRequester, pvRequest);
                epicsThreadSleep ( SLEEP_TIME );
                channelProcess->process();
                epicsThreadSleep ( SLEEP_TIME );
                channelProcess->destroy();
                epicsThreadSleep ( SLEEP_TIME );
            }

            {
                ChannelGetRequester::shared_pointer channelGetRequesterImpl(new ChannelGetRequesterImpl());
                PVStructure::shared_pointer pvRequest = CreateRequest::create()->createRequest("field()");
                ChannelGet::shared_pointer channelGet = channel->createChannelGet(channelGetRequesterImpl, pvRequest);
                epicsThreadSleep ( 3.0 );
                channelGet->get();
                epicsThreadSleep ( 3.0 );
                channelGet->destroy();
            }

            {
                ChannelPutRequester::shared_pointer channelPutRequesterImpl(new ChannelPutRequesterImpl());
                PVStructure::shared_pointer pvRequest = CreateRequest::create()->createRequest("field(value,timeStamp)");
                ChannelPut::shared_pointer channelPut = channel->createChannelPut(channelPutRequesterImpl, pvRequest);
                epicsThreadSleep ( SLEEP_TIME );
                channelPut->get();
                epicsThreadSleep ( SLEEP_TIME );
                // TODO !!!
                //channelPut->put();
                //epicsThreadSleep ( SLEEP_TIME );
                channelPut->destroy();
            }

            {
                ChannelPutGetRequester::shared_pointer channelPutGetRequesterImpl(new ChannelPutGetRequesterImpl());
                PVStructure::shared_pointer pvRequest = CreateRequest::create()->createRequest("putField(value,timeStamp)getField(timeStamp)");
                ChannelPutGet::shared_pointer channelPutGet = channel->createChannelPutGet(channelPutGetRequesterImpl, pvRequest);
                epicsThreadSleep ( SLEEP_TIME );
                channelPutGet->getGet();
                epicsThreadSleep ( SLEEP_TIME );
                channelPutGet->getPut();
                epicsThreadSleep ( SLEEP_TIME );
                // TODO !!!
                //channelPutGet->putGet();
                //epicsThreadSleep ( SLEEP_TIME );
                channelPutGet->destroy();
            }

            {
                ChannelRPCRequester::shared_pointer channelRPCRequesterImpl(new ChannelRPCRequesterImpl());
                PVStructure::shared_pointer pvRequest = CreateRequest::create()->createRequest("record[]field(arguments)");
                ChannelRPC::shared_pointer channelRPC = channel->createChannelRPC(channelRPCRequesterImpl, pvRequest);
                epicsThreadSleep ( SLEEP_TIME );
                // for test simply use pvRequest as arguments
                channelRPC->request(pvRequest);
                epicsThreadSleep ( SLEEP_TIME );
                channelRPC->destroy();
            }

            {
                ChannelArrayRequester::shared_pointer channelArrayRequesterImpl(new ChannelArrayRequesterImpl());
                StringArray fieldNames;
                fieldNames.push_back("field");
                FieldConstPtrArray fields;
                fields.push_back(getFieldCreate()->createScalar(pvString));
                PVStructure::shared_pointer pvRequest(getPVDataCreate()->createPVStructure(getFieldCreate()->createStructure(fieldNames, fields)));

                ChannelArray::shared_pointer channelArray = channel->createChannelArray(channelArrayRequesterImpl, pvRequest);
                epicsThreadSleep ( SLEEP_TIME );
                channelArray->getArray(0,0,1);
                epicsThreadSleep ( SLEEP_TIME );
                // TODO !!!
                //channelArray->putArray(0,0,1);
                //epicsThreadSleep ( SLEEP_TIME );
                channelArray->setLength(3);
                epicsThreadSleep ( SLEEP_TIME );
                channelArray->getLength();
                epicsThreadSleep ( SLEEP_TIME );
                channelArray->destroy();
            }

            {
                MonitorRequester::shared_pointer monitorRequesterImpl(new MonitorRequesterImpl());
                PVStructure::shared_pointer pvRequest = CreateRequest::create()->createRequest("field()");
                Monitor::shared_pointer monitor = channel->createMonitor(monitorRequesterImpl, pvRequest);

                epicsThreadSleep( SLEEP_TIME );

                Status status = monitor->start();
                std::cout << "monitor->start() = " << status << std::endl;

                epicsThreadSleep( 3*SLEEP_TIME );

                status = monitor->stop();
                std::cout << "monitor->stop() = " << status << std::endl;


                monitor->destroy();
            }

            epicsThreadSleep ( 3*SLEEP_TIME );
            printf("Destroying channel... \n");
            channel->destroy();
            printf("done.\n");

            epicsThreadSleep ( 3*SLEEP_TIME );

        }

        ClientFactory::stop();

        /*
        printf("Destroying context... \n");
        context->destroy();
        printf("done.\n");
        */

        epicsThreadSleep ( SLEEP_TIME );
    }
    //std::cout << "-----------------------------------------------------------------------" << std::endl;
    //epicsExitCallAtExits();
    return(0);
}
