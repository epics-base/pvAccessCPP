
/* testRemoteClientImpl.cpp */
/* Author:  Matej Sekoranja Date: 2011.1.1 */


#include <iostream>
#include <sstream>
#include <CDRMonitor.h>
#include <epicsExit.h>
#include <clientContextImpl.h>
#include <clientFactory.h>

using namespace epics::pvData;
using namespace epics::pvAccess;


class ChannelFindRequesterImpl : public ChannelFindRequester
{
    virtual void channelFindResult(epics::pvData::Status *status,ChannelFind *channelFind,bool wasFound)
    {
        std::cout << "[ChannelFindRequesterImpl] channelFindResult("
                  << status->toString() << ", ..., " << wasFound << ")" << std::endl;
    }
};

class ChannelRequesterImpl : public ChannelRequester
{
    virtual String getRequesterName()
    {
        return "ChannelRequesterImpl";
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    virtual void channelCreated(epics::pvData::Status* status, Channel *channel)
    {
        std::cout << "channelCreated(" << status->toString() << ", "
                  << (channel ? channel->getChannelName() : "(0)") << ")" << std::endl;
    }

    virtual void channelStateChange(Channel *c, Channel::ConnectionState connectionState)
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

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    virtual void getDone(epics::pvData::Status *status,epics::pvData::FieldConstPtr field)
    {
        std::cout << "getDone(" << status->toString() << ", ";
        if (field)
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
    Mutex m_mutex;
    ChannelGet *m_channelGet;
    epics::pvData::PVStructure *m_pvStructure;
    epics::pvData::BitSet *m_bitSet;

    public:
    
    ChannelGetRequesterImpl() : m_channelGet(0), m_pvStructure(0), m_bitSet(0) {}
    
    virtual String getRequesterName()
    {
        return "ChannelGetRequesterImpl";
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    virtual void channelGetConnect(epics::pvData::Status *status,ChannelGet *channelGet,
            epics::pvData::PVStructure *pvStructure,epics::pvData::BitSet *bitSet)
    {
        std::cout << "channelGetConnect(" << status->toString() << ")" << std::endl;
        if (pvStructure)
        {
        	String st;
        	pvStructure->toString(&st);
            std::cout << st << std::endl;
        }
        
        m_mutex.lock();
        m_channelGet = channelGet;
        m_pvStructure = pvStructure;
        m_bitSet = bitSet;
        m_mutex.unlock();
    }

    virtual void getDone(epics::pvData::Status *status)
    {
        std::cout << "getDone(" << status->toString() << ")" << std::endl;
        Lock guard(&m_mutex);
        if (m_pvStructure)
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
    ChannelPut *m_channelPut;
    epics::pvData::PVStructure *m_pvStructure;
    epics::pvData::BitSet *m_bitSet;

    virtual String getRequesterName()
    {
        return "ChannelPutRequesterImpl";
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    virtual void channelPutConnect(epics::pvData::Status *status,ChannelPut *channelPut,
            epics::pvData::PVStructure *pvStructure,epics::pvData::BitSet *bitSet)
    {
        std::cout << "channelPutConnect(" << status->toString() << ")" << std::endl;

        // TODO sync
        m_channelPut = channelPut;
        m_pvStructure = pvStructure;
        m_bitSet = bitSet;
    }

    virtual void getDone(epics::pvData::Status *status)
    {
        std::cout << "getDone(" << status->toString() << ")" << std::endl;
        if (m_pvStructure)
        {
            String str;
            m_pvStructure->toString(&str);
            std::cout << str;
            std::cout << std::endl;
        }
    }

    virtual void putDone(epics::pvData::Status *status)
    {
        std::cout << "putDone(" << status->toString() << ")" << std::endl;
        if (m_pvStructure)
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
    ChannelPutGet *m_channelPutGet;
    epics::pvData::PVStructure *m_putData;
    epics::pvData::PVStructure *m_getData;

    virtual String getRequesterName()
    {
        return "ChannelGetPutRequesterImpl";
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    virtual void channelPutGetConnect(epics::pvData::Status *status,ChannelPutGet *channelPutGet,
            epics::pvData::PVStructure *putData,epics::pvData::PVStructure *getData)
    {
        std::cout << "channelGetPutConnect(" << status->toString() << ")" << std::endl;
        // TODO sync
        m_channelPutGet = channelPutGet;
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

    virtual void getGetDone(epics::pvData::Status *status)
    {
        std::cout << "getGetDone(" << status->toString() << ")" << std::endl;
        if (m_getData)
        {
            String str;
            m_getData->toString(&str);
            std::cout << str;
            std::cout << std::endl;
        }
    }

    virtual void getPutDone(epics::pvData::Status *status)
    {
        std::cout << "getPutDone(" << status->toString() << ")" << std::endl;
        if (m_putData)
        {
            String str;
            m_putData->toString(&str);
            std::cout << str;
            std::cout << std::endl;
        }
    }

    virtual void putGetDone(epics::pvData::Status *status)
    {
        std::cout << "putGetDone(" << status->toString() << ")" << std::endl;
        if (m_putData)
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
    ChannelRPC *m_channelRPC;
    epics::pvData::PVStructure *m_pvStructure;
    epics::pvData::BitSet *m_bitSet;

    virtual String getRequesterName()
    {
        return "ChannelRPCRequesterImpl";
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    virtual void channelRPCConnect(epics::pvData::Status *status,ChannelRPC *channelRPC,
            epics::pvData::PVStructure *pvStructure,epics::pvData::BitSet *bitSet)
    {
        std::cout << "channelRPCConnect(" << status->toString() << ")" << std::endl;
        if (pvStructure)
        {
        	String st;
        	pvStructure->toString(&st);
            std::cout << st << std::endl;
        }
        
        // TODO sync
        m_channelRPC = channelRPC;
        m_pvStructure = pvStructure;
        m_bitSet = bitSet;
    }

    virtual void requestDone(epics::pvData::Status *status,epics::pvData::PVStructure *pvResponse)
    {
        std::cout << "requestDone(" << status->toString() << ")" << std::endl;
        if (pvResponse)
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
    ChannelArray *m_channelArray;
    epics::pvData::PVArray *m_pvArray;

    virtual String getRequesterName()
    {
        return "ChannelArrayRequesterImpl";
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    virtual void channelArrayConnect(epics::pvData::Status *status,ChannelArray *channelArray,
            epics::pvData::PVArray *pvArray)
    {
        std::cout << "channelArrayConnect(" << status->toString() << ")" << std::endl;
        if (pvArray)
        {
        	String st;
        	pvArray->toString(&st);
            std::cout << st << std::endl;
        }
        
        // TODO sync
        m_channelArray = channelArray;
        m_pvArray = pvArray;
    }

    virtual void getArrayDone(epics::pvData::Status *status)
    {
        std::cout << "getArrayDone(" << status->toString() << ")" << std::endl;
        if (m_pvArray)
        {
            String str;
            m_pvArray->toString(&str);
            std::cout << str;
            std::cout << std::endl;
        }
    }

    virtual void putArrayDone(epics::pvData::Status *status)
    {
        std::cout << "putArrayDone(" << status->toString() << ")" << std::endl;
    }

    virtual void setLengthDone(epics::pvData::Status *status)
    {
        std::cout << "setLengthDone(" << status->toString() << ")" << std::endl;
    }
};

class MonitorRequesterImpl : public MonitorRequester
{
    virtual String getRequesterName()
    {
        return "MonitorRequesterImpl";
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    virtual void monitorConnect(Status* status, Monitor* monitor, Structure* structure)
    {
        std::cout << "monitorConnect(" << status->toString() << ")" << std::endl;
        if (structure)
        {
            String str;
            structure->toString(&str);
            std::cout << str << std::endl;
        }
    }

    virtual void monitorEvent(Monitor* monitor)
    {
        std::cout << "monitorEvent" << std::endl;

        MonitorElement* element = monitor->poll();

        String str("changed/overrun ");
        element->getChangedBitSet()->toString(&str);
        str += '/';
        element->getOverrunBitSet()->toString(&str);
        str += '\n';
        element->getPVStructure()->toString(&str);
        std::cout << str << std::endl;

        monitor->release(element);
    }

    virtual void unlisten(Monitor* monitor)
    {
        std::cout << "unlisten" << std::endl;
    }
};


class ChannelProcessRequesterImpl : public ChannelProcessRequester
{
    ChannelProcess *m_channelProcess;

    virtual String getRequesterName()
    {
        return "ProcessRequesterImpl";
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    virtual void channelProcessConnect(epics::pvData::Status *status,ChannelProcess *channelProcess)
    {
        std::cout << "channelProcessConnect(" << status->toString() << ")" << std::endl;

        // TODO sync
        m_channelProcess = channelProcess;
    }

    virtual void processDone(epics::pvData::Status *status)
    {
        std::cout << "processDone(" << status->toString() << ")" << std::endl;
    }

};


int main(int argc,char *argv[])
{
    /*
    ClientContextImpl* context = createClientContextImpl();
    context->printInfo();

    context->initialize();
    context->printInfo();

    epicsThreadSleep ( 1.0 );
    
    ChannelProvider* provider = context->getProvider();
    */
    
    ClientFactory::start();
    ChannelProvider* provider = getChannelAccess()->getProvider("pvAccess");

/*
    ChannelFindRequesterImpl findRequester;
    ChannelFind* channelFind = provider->channelFind("something", &findRequester);
    epicsThreadSleep ( 1.0 );
    channelFind->destroy();
*/
    ChannelRequesterImpl channelRequester;
    Channel* channel = provider->createChannel("structureArrayTest", &channelRequester);

    epicsThreadSleep ( 1.0 );

    channel->printInfo();
    
    PVStructure* pvRequest;

    GetFieldRequesterImpl getFieldRequesterImpl;
    channel->getField(&getFieldRequesterImpl, "");
    epicsThreadSleep ( 1.0 );

    ChannelProcessRequesterImpl channelProcessRequester;
    ChannelProcess* channelProcess = channel->createChannelProcess(&channelProcessRequester, 0);
    epicsThreadSleep ( 1.0 );
    channelProcess->process(false);
    epicsThreadSleep ( 1.0 );
    channelProcess->destroy();
    epicsThreadSleep ( 1.0 );

    ChannelGetRequesterImpl channelGetRequesterImpl;
    pvRequest = getCreateRequest()->createRequest("field()",&channelGetRequesterImpl);
    ChannelGet* channelGet = channel->createChannelGet(&channelGetRequesterImpl, pvRequest);
    epicsThreadSleep ( 3.0 );
    channelGet->get(false);
    epicsThreadSleep ( 3.0 );
    
    channelGet->destroy();
    delete pvRequest;

    ChannelPutRequesterImpl channelPutRequesterImpl;
    pvRequest = getCreateRequest()->createRequest("field(value,timeStamp)",&channelPutRequesterImpl);
    ChannelPut* channelPut = channel->createChannelPut(&channelPutRequesterImpl, pvRequest);
    epicsThreadSleep ( 1.0 );
    channelPut->get();
    epicsThreadSleep ( 1.0 );
    channelPut->put(false);
    epicsThreadSleep ( 1.0 );
    channelPut->destroy();
    delete pvRequest;

    ChannelPutGetRequesterImpl channelPutGetRequesterImpl;
    pvRequest = getCreateRequest()->createRequest("putField(value,timeStamp)getField(timeStamp)",&channelPutGetRequesterImpl);
    ChannelPutGet* channelPutGet = channel->createChannelPutGet(&channelPutGetRequesterImpl, pvRequest);
    epicsThreadSleep ( 1.0 );
    channelPutGet->getGet();
    epicsThreadSleep ( 1.0 );
    channelPutGet->getPut();
    epicsThreadSleep ( 1.0 );
    channelPutGet->putGet(false);
    epicsThreadSleep ( 1.0 );
    channelPutGet->destroy();
    delete pvRequest;


    ChannelRPCRequesterImpl channelRPCRequesterImpl;
    pvRequest = getCreateRequest()->createRequest("record[]field(arguments)",&channelRPCRequesterImpl);
    ChannelRPC* channelRPC = channel->createChannelRPC(&channelRPCRequesterImpl, pvRequest);
    epicsThreadSleep ( 1.0 );
    channelRPC->request(false);
    epicsThreadSleep ( 1.0 );
    channelRPC->destroy();
    delete pvRequest;

    ChannelArrayRequesterImpl channelArrayRequesterImpl;
    //pvRequest = getCreateRequest()->createRequest("value",&channelArrayRequesterImpl);
    pvRequest = getPVDataCreate()->createPVStructure(0, "", 0);
    PVString* pvFieldName = (PVString*)getPVDataCreate()->createPVScalar(pvRequest, "field", pvString);
    pvFieldName->put("value");
    pvRequest->appendPVField(pvFieldName);

    ChannelArray* channelArray = channel->createChannelArray(&channelArrayRequesterImpl, pvRequest);
    epicsThreadSleep ( 1.0 );
    channelArray->getArray(false,0,-1);
    epicsThreadSleep ( 1.0 );
    channelArray->putArray(false,0,-1);
    epicsThreadSleep ( 1.0 );
    channelArray->setLength(false,3,4);
    epicsThreadSleep ( 1.0 );
    channelArray->destroy();
    delete pvRequest;

    MonitorRequesterImpl monitorRequesterImpl;
    pvRequest = getCreateRequest()->createRequest("field()",&monitorRequesterImpl);
    Monitor* monitor = channel->createMonitor(&monitorRequesterImpl, pvRequest);

    epicsThreadSleep( 1.0 );

    Status* status = monitor->start();
    std::cout << "monitor->start() = " << status->toString() << std::endl;
    delete status;

    epicsThreadSleep( 3.0 );

    status = monitor->stop();
    std::cout << "monitor->stop() = " << status->toString() << std::endl;
    delete status;


    monitor->destroy();
    delete pvRequest;
   
    epicsThreadSleep ( 3.0 );
    printf("Destroying channel... \n");
    channel->destroy();
    printf("done.\n");

    epicsThreadSleep ( 3.0 );

    ClientFactory::stop();
    /*
    printf("Destroying context... \n");
    context->destroy();
    printf("done.\n");
    */
    
    epicsThreadSleep ( 1.0 );
    std::cout << "-----------------------------------------------------------------------" << std::endl;
    epicsExitCallAtExits();
    CDRMonitor::get().show(stdout);
    return(0);
}
