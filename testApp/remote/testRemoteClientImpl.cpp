
/* testRemoteClientImpl.cpp */
/* Author:  Matej Sekoranja Date: 2011.1.1 */


#include <iostream>
#include <sstream>
#include <epicsExit.h>
#include <pv/clientContextImpl.h>
#include <pv/clientFactory.h>

using namespace epics::pvData;
using namespace epics::pvAccess;


class ChannelFindRequesterImpl : public ChannelFindRequester
{
    virtual void channelFindResult(const epics::pvData::Status& status,
    		ChannelFind::shared_pointer const & /*channelFind*/,
    		bool wasFound)
    {
        std::cout << "[ChannelFindRequesterImpl] channelFindResult("
                  << status.toString() << ", ..., " << wasFound << ")" << std::endl;
    }
};

class ChannelRequesterImpl : public ChannelRequester
{
    virtual String getRequesterName()
    {
        return "ChannelRequesterImpl";
    };

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelCreated(const epics::pvData::Status& status, Channel::shared_pointer const & channel)
    {
        std::cout << "channelCreated(" << status.toString() << ", "
                  << (channel ? channel->getChannelName() : "(0)") << ")" << std::endl;
    }

    virtual void channelStateChange(Channel::shared_pointer const & c, Channel::ConnectionState connectionState)
    {
        std::cout << "channelStateChange(" << c->getChannelName() << ", " << Channel::ConnectionStateNames[connectionState] << ")" << std::endl;
    }
};

class GetFieldRequesterImpl : public GetFieldRequester
{
    virtual String getRequesterName()
    {
        return "GetFieldRequesterImpl";
    };

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void getDone(const epics::pvData::Status& status,epics::pvData::FieldConstPtr const & field)
    {
        std::cout << "getDone(" << status.toString() << ", ";
        if (status.isSuccess() && field)
        {
            String str;
            field->toString(&str);
            std::cout << str;
        }
        else
            std::cout << "(0)";
        std::cout << ")" << std::endl;
    }
};

class ChannelGetRequesterImpl : public ChannelGetRequester
{
    private:
    
    //ChannelGet::shared_pointer m_channelGet;
    epics::pvData::PVStructure::shared_pointer m_pvStructure;
    epics::pvData::BitSet::shared_pointer m_bitSet;

    public:
    
    virtual String getRequesterName()
    {
        return "ChannelGetRequesterImpl";
    };

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelGetConnect(const epics::pvData::Status& status,ChannelGet::shared_pointer const & /*channelGet*/,
                                   epics::pvData::PVStructure::shared_pointer const & pvStructure,
                                   epics::pvData::BitSet::shared_pointer const & bitSet)
    {
        std::cout << "channelGetConnect(" << status.toString() << ")" << std::endl;
        if (status.isSuccess())
        {
        	String st;
        	pvStructure->toString(&st);
            std::cout << st << std::endl;
        }
        
        //m_channelGet = channelGet;
        m_pvStructure = pvStructure;
        m_bitSet = bitSet;
    }

    virtual void getDone(const epics::pvData::Status& status)
    {
        std::cout << "getDone(" << status.toString() << ")" << std::endl;

        if (status.isSuccess())
        {
            String str;
            m_pvStructure->toString(&str);
            std::cout << str;
            std::cout << std::endl;
        }
    }
};

class ChannelPutRequesterImpl : public ChannelPutRequester
{
    private:
    
    //ChannelPut::shared_pointer m_channelPut;
    epics::pvData::PVStructure::shared_pointer m_pvStructure;
    epics::pvData::BitSet::shared_pointer m_bitSet;

    public: 
    
    virtual String getRequesterName()
    {
        return "ChannelPutRequesterImpl";
    };

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelPutConnect(const epics::pvData::Status& status,ChannelPut::shared_pointer const & /*channelPut*/,
                                   epics::pvData::PVStructure::shared_pointer const & pvStructure,
                                   epics::pvData::BitSet::shared_pointer const & bitSet)
    {
        std::cout << "channelPutConnect(" << status.toString() << ")" << std::endl;

        //m_channelPut = channelPut;
        m_pvStructure = pvStructure;
        m_bitSet = bitSet;
    }

    virtual void getDone(const epics::pvData::Status& status)
    {
        std::cout << "getDone(" << status.toString() << ")" << std::endl;
        if (status.isSuccess())
        {
            String str;
            m_pvStructure->toString(&str);
            std::cout << str;
            std::cout << std::endl;
        }
    }

    virtual void putDone(const epics::pvData::Status& status)
    {
        std::cout << "putDone(" << status.toString() << ")" << std::endl;
        if (status.isSuccess())
        {
            String str;
            m_pvStructure->toString(&str);
            std::cout << str;
            std::cout << std::endl;
        }
    }

};

class ChannelPutGetRequesterImpl : public ChannelPutGetRequester
{
    private:
    
    //ChannelPutGet::shared_pointer m_channelPutGet;
    epics::pvData::PVStructure::shared_pointer m_putData;
    epics::pvData::PVStructure::shared_pointer m_getData;

    public:

    virtual String getRequesterName()
    {
        return "ChannelGetPutRequesterImpl";
    };

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelPutGetConnect(const epics::pvData::Status& status, ChannelPutGet::shared_pointer const & /*channelPutGet*/,
                                      epics::pvData::PVStructure::shared_pointer const & putData,
                                      epics::pvData::PVStructure::shared_pointer const & getData)
    {
        std::cout << "channelGetPutConnect(" << status.toString() << ")" << std::endl;

        //m_channelPutGet = channelPutGet;
        m_putData = putData;
        m_getData = getData;

        if (m_putData)
        {
            String str;
            m_putData->toString(&str);
            std::cout << str;
            std::cout << std::endl;
        }
        if (m_getData)
        {
            String str;
            m_getData->toString(&str);
            std::cout << str;
            std::cout << std::endl;
        }
    }

    virtual void getGetDone(const epics::pvData::Status& status)
    {
        std::cout << "getGetDone(" << status.toString() << ")" << std::endl;
        if (status.isSuccess())
        {
            String str;
            m_getData->toString(&str);
            std::cout << str;
            std::cout << std::endl;
        }
    }

    virtual void getPutDone(const epics::pvData::Status& status)
    {
        std::cout << "getPutDone(" << status.toString() << ")" << std::endl;
        if (status.isSuccess())
        {
            String str;
            m_putData->toString(&str);
            std::cout << str;
            std::cout << std::endl;
        }
    }

    virtual void putGetDone(const epics::pvData::Status& status)
    {
        std::cout << "putGetDone(" << status.toString() << ")" << std::endl;
        if (status.isSuccess())
        {
            String str;
            m_putData->toString(&str);
            std::cout << str;
            std::cout << std::endl;
        }
    }

};


class ChannelRPCRequesterImpl : public ChannelRPCRequester
{
    //ChannelRPC::shared_pointer m_channelRPC;

    virtual String getRequesterName()
    {
        return "ChannelRPCRequesterImpl";
    };

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelRPCConnect(const epics::pvData::Status& status,
    		ChannelRPC::shared_pointer const & /*channelRPC*/)
    {
        std::cout << "channelRPCConnect(" << status.toString() << ")" << std::endl;
        
        //m_channelRPC = channelRPC;
    }

    virtual void requestDone(const epics::pvData::Status& status,epics::pvData::PVStructure::shared_pointer const & pvResponse)
    {
        std::cout << "requestDone(" << status.toString() << ")" << std::endl;
        if (status.isSuccess())
        {
            String str;
            pvResponse->toString(&str);
            std::cout << str;
            std::cout << std::endl;
        }
    }
};

class ChannelArrayRequesterImpl : public ChannelArrayRequester
{
    private:
    
    //ChannelArray::shared_pointer m_channelArray;
    epics::pvData::PVArray::shared_pointer m_pvArray;

    public:
    
    virtual String getRequesterName()
    {
        return "ChannelArrayRequesterImpl";
    };

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelArrayConnect(const epics::pvData::Status& status,ChannelArray::shared_pointer const & /*channelArray*/,
                                     epics::pvData::PVArray::shared_pointer const & pvArray)
    {
        std::cout << "channelArrayConnect(" << status.toString() << ")" << std::endl;
        if (status.isSuccess())
        {
        	String st;
        	pvArray->toString(&st);
            std::cout << st << std::endl;
        }
        
        //m_channelArray = channelArray;
        m_pvArray = pvArray;
    }

    virtual void getArrayDone(const epics::pvData::Status& status)
    {
        std::cout << "getArrayDone(" << status.toString() << ")" << std::endl;
        if (status.isSuccess())
        {
            String str;
            m_pvArray->toString(&str);
            std::cout << str;
            std::cout << std::endl;
        }
    }

    virtual void putArrayDone(const epics::pvData::Status& status)
    {
        std::cout << "putArrayDone(" << status.toString() << ")" << std::endl;
    }

    virtual void setLengthDone(const epics::pvData::Status& status)
    {
        std::cout << "setLengthDone(" << status.toString() << ")" << std::endl;
    }
};

class MonitorRequesterImpl : public MonitorRequester
{
    virtual String getRequesterName()
    {
        return "MonitorRequesterImpl";
    };

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void monitorConnect(const epics::pvData::Status& status,
    		Monitor::shared_pointer const & /*monitor*/, StructureConstPtr const & structure)
    {
        std::cout << "monitorConnect(" << status.toString() << ")" << std::endl;
        if (status.isSuccess() && structure)
        {
            String str;
            structure->toString(&str);
            std::cout << str << std::endl;
        }
    }

    virtual void monitorEvent(Monitor::shared_pointer const & monitor)
    {
        std::cout << "monitorEvent" << std::endl;

        MonitorElement::shared_pointer element = monitor->poll();

        String str("changed/overrun ");
        element->changedBitSet->toString(&str);
        str += '/';
        element->overrunBitSet->toString(&str);
        str += '\n';
        element->pvStructurePtr->toString(&str);
        std::cout << str << std::endl;

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

    virtual String getRequesterName()
    {
        return "ProcessRequesterImpl";
    };

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelProcessConnect(const epics::pvData::Status& status,
    		ChannelProcess::shared_pointer const & /*channelProcess*/)
    {
        std::cout << "channelProcessConnect(" << status.toString() << ")" << std::endl;

        //m_channelProcess = channelProcess;
    }

    virtual void processDone(const epics::pvData::Status& status)
    {
        std::cout << "processDone(" << status.toString() << ")" << std::endl;
    }

};


int main()
{
    for (int i = 0; i < 10; i++) {
    {
    /*
    ClientContextImpl::shared_pointer context = createClientContextImpl();
    context->printInfo();

    context->initialize();
    context->printInfo();

    epicsThreadSleep ( 1.0 );
    
    ChannelProvider::shared_pointer provider = context->getProvider();
    */
    
    ClientFactory::start();
    ChannelProvider::shared_pointer provider = getChannelAccess()->getProvider("pvAccess");

    ChannelFindRequester::shared_pointer findRequester(new ChannelFindRequesterImpl());
    ChannelFind::shared_pointer channelFind = provider->channelFind("testSomething", findRequester);
    epicsThreadSleep ( 1.0 );
    //channelFind->destroy();

    ChannelRequester::shared_pointer channelRequester(new ChannelRequesterImpl());
    Channel::shared_pointer channel = provider->createChannel("testStructureArrayTest", channelRequester);
    epicsThreadSleep ( 1.0 );
    channel->printInfo();
        
        {
    GetFieldRequester::shared_pointer getFieldRequesterImpl(new GetFieldRequesterImpl());
    channel->getField(getFieldRequesterImpl, "");
    epicsThreadSleep ( 1.0 );
        }
        
        {
    ChannelProcessRequester::shared_pointer channelProcessRequester(new ChannelProcessRequesterImpl());
    PVStructure::shared_pointer pvRequest;
    ChannelProcess::shared_pointer channelProcess = channel->createChannelProcess(channelProcessRequester, pvRequest);
    epicsThreadSleep ( 1.0 );
    channelProcess->process(false);
    epicsThreadSleep ( 1.0 );
    channelProcess->destroy();
    epicsThreadSleep ( 1.0 );
        }
        
        {
    ChannelGetRequester::shared_pointer channelGetRequesterImpl(new ChannelGetRequesterImpl());
    PVStructure::shared_pointer pvRequest = getCreateRequest()->createRequest("field()",channelGetRequesterImpl);
    ChannelGet::shared_pointer channelGet = channel->createChannelGet(channelGetRequesterImpl, pvRequest);
    epicsThreadSleep ( 3.0 );
    channelGet->get(false);
    epicsThreadSleep ( 3.0 );
    channelGet->destroy();
        }
    
        {
    ChannelPutRequester::shared_pointer channelPutRequesterImpl(new ChannelPutRequesterImpl());
    PVStructure::shared_pointer pvRequest = getCreateRequest()->createRequest("field(value,timeStamp)",channelPutRequesterImpl);
    ChannelPut::shared_pointer channelPut = channel->createChannelPut(channelPutRequesterImpl, pvRequest);
    epicsThreadSleep ( 1.0 );
    channelPut->get();
    epicsThreadSleep ( 1.0 );
    channelPut->put(false);
    epicsThreadSleep ( 1.0 );
    channelPut->destroy();
        }

        {
    ChannelPutGetRequester::shared_pointer channelPutGetRequesterImpl(new ChannelPutGetRequesterImpl());
    PVStructure::shared_pointer pvRequest = getCreateRequest()->createRequest("putField(value,timeStamp)getField(timeStamp)",channelPutGetRequesterImpl);
    ChannelPutGet::shared_pointer channelPutGet = channel->createChannelPutGet(channelPutGetRequesterImpl, pvRequest);
    epicsThreadSleep ( 1.0 );
    channelPutGet->getGet();
    epicsThreadSleep ( 1.0 );
    channelPutGet->getPut();
    epicsThreadSleep ( 1.0 );
    channelPutGet->putGet(false);
    epicsThreadSleep ( 1.0 );
    channelPutGet->destroy();
        }

        {
    ChannelRPCRequester::shared_pointer channelRPCRequesterImpl(new ChannelRPCRequesterImpl());
    PVStructure::shared_pointer pvRequest = getCreateRequest()->createRequest("record[]field(arguments)",channelRPCRequesterImpl);
    ChannelRPC::shared_pointer channelRPC = channel->createChannelRPC(channelRPCRequesterImpl, pvRequest);
    epicsThreadSleep ( 1.0 );
    // for test simply use pvRequest as arguments
    channelRPC->request(pvRequest, false);
    epicsThreadSleep ( 1.0 );
    channelRPC->destroy();
        }
         
        {
    ChannelArrayRequester::shared_pointer channelArrayRequesterImpl(new ChannelArrayRequesterImpl());
    StringArray fieldNames; fieldNames.push_back("field");
    FieldConstPtrArray fields; fields.push_back(getFieldCreate()->createScalar(pvString));
    PVStructure::shared_pointer pvRequest(getPVDataCreate()->createPVStructure(getFieldCreate()->createStructure(fieldNames, fields)));

    ChannelArray::shared_pointer channelArray = channel->createChannelArray(channelArrayRequesterImpl, pvRequest);
    epicsThreadSleep ( 1.0 );
    channelArray->getArray(false,0,-1);
    epicsThreadSleep ( 1.0 );
    channelArray->putArray(false,0,-1);
    epicsThreadSleep ( 1.0 );
    channelArray->setLength(false,3,4);
    epicsThreadSleep ( 1.0 );
    channelArray->destroy();
        }

        {
    MonitorRequester::shared_pointer monitorRequesterImpl(new MonitorRequesterImpl());
    PVStructure::shared_pointer pvRequest = getCreateRequest()->createRequest("field()",monitorRequesterImpl);
    Monitor::shared_pointer monitor = channel->createMonitor(monitorRequesterImpl, pvRequest);

    epicsThreadSleep( 1.0 );

    Status status = monitor->start();
    std::cout << "monitor->start() = " << status.toString() << std::endl;

    epicsThreadSleep( 3.0 );

    status = monitor->stop();
    std::cout << "monitor->stop() = " << status.toString() << std::endl;


    monitor->destroy();
        }
    
    epicsThreadSleep ( 3.0 );
    printf("Destroying channel... \n");
    channel->destroy();
    printf("done.\n");

    epicsThreadSleep ( 3.0 );
        
    }
    
    ClientFactory::stop();

    /*
    printf("Destroying context... \n");
    context->destroy();
    printf("done.\n");
    */
    
    epicsThreadSleep ( 1.0 ); }
    //std::cout << "-----------------------------------------------------------------------" << std::endl;
    //epicsExitCallAtExits();
    return(0);
}
