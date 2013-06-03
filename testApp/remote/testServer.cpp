/*
 * testServer.cpp
 */

#include <pv/serverContext.h>
#include <pv/clientContextImpl.h>
#include <epicsExit.h>
#include <pv/standardPVField.h>
#include <pv/pvTimeStamp.h>

#include <stdlib.h>
#include <time.h>
#include <vector>
#include <map>

#include <pv/logger.h>

// TODO temp
#include "testADCSim.cpp"


using namespace epics::pvAccess;
using namespace epics::pvData;
using namespace std;
using std::tr1::static_pointer_cast;
using std::tr1::dynamic_pointer_cast;

// forward declaration
void testServerShutdown();

// TODO temp
#include "testNTImage.cpp"

map<String, PVStructure::shared_pointer> structureStore;

class StructureChangedCallback {
public:
    POINTER_DEFINITIONS(StructureChangedCallback);
    // TODO for now no BitSets, etc.
    virtual void structureChanged() = 0;
};

map<String, vector<StructureChangedCallback::shared_pointer> > structureChangedListeners;

static void notifyStructureChanged(String const & name)
{
    // NOTE: not thread-safe
    if (structureChangedListeners.find(name) != structureChangedListeners.end())
    {
        vector<StructureChangedCallback::shared_pointer> & vec = structureChangedListeners[name];
        for (vector<StructureChangedCallback::shared_pointer>::iterator iter = vec.begin();
             iter != vec.end();
             iter++)
        {
            (*iter)->structureChanged();
        }
    }
}

static PVStructure::shared_pointer getRequestedStructure(
        PVStructure::shared_pointer const & pvStructure,
        PVStructure::shared_pointer const & pvRequest,
        String subfieldName = "field")
{
    // if pvRequest is empty, just use pvStructure
    if (pvRequest.get() && pvRequest->getPVFields().size() > 0)
    {
        PVStructure::shared_pointer pvRequestFields;
        if (pvRequest->getSubField(subfieldName))
            pvRequestFields = pvRequest->getStructureField(subfieldName);
        else
            pvRequestFields = pvRequest;

        // if pvRequest is empty, just use pvStructure
        if (pvRequestFields->getPVFields().size() > 0)
        {
            StringArray const & fieldNames = pvRequestFields->getStructure()->getFieldNames();
            PVFieldPtrArray pvFields;
            StringArray actualFieldNames;
            for (StringArray::const_iterator iter = fieldNames.begin();
                 iter != fieldNames.end();
                 iter++)
            {
                PVFieldPtr pvField = pvStructure->getSubField(*iter);
                if (pvField)
                {
                    PVStructurePtr pvFieldStructure =
                            std::tr1::dynamic_pointer_cast<PVStructure>(pvField);

                    PVStructurePtr pvRequestFieldStructure =
                            std::tr1::dynamic_pointer_cast<PVStructure>(pvRequestFields->getSubField(*iter));
                    if (pvRequestFieldStructure->getPVFields().size() > 0 && pvFieldStructure.get())
                    {
                        // add subfields only
                        actualFieldNames.push_back(*iter);
                        pvFields.push_back(getRequestedStructure(pvFieldStructure, pvRequestFieldStructure));
                    }
                    else
                    {
                        // add entire field
                        actualFieldNames.push_back(*iter);
                        pvFields.push_back(pvField);
                    }
                }
            }

            return getPVDataCreate()->createPVStructure(actualFieldNames, pvFields);
        }
    }

    return pvStructure;
}


class ProcessAction : public Runnable {
public:
    typedef vector<ChannelProcess::shared_pointer> ChannelProcessVector;
    ChannelProcessVector toProcess;
    AtomicBoolean stopped;
    double period;

    ProcessAction(double periodHz) : period(periodHz) {}

    virtual void run()
    {
        while (!stopped.get())
        {

            for (ChannelProcessVector::iterator iter = toProcess.begin();
                 iter != toProcess.end();
                 iter++)
            {
                try {
                    (*iter)->process(false);
                } catch (std::exception &ex) {
                    std::cerr << "Unhandled exception caught in ProcessAction::run(): " << ex.what() << std::endl;
                } catch (...) {
                    std::cerr << "Unhandled exception caught in ProcessAction::run()" << std::endl;
                }

                epicsThreadSleep(period);
            }
        }
    }
};

// ADC
class ADCAction : public Runnable {
public:
    String name;
    epics::pvData::PVStructure::shared_pointer adcMatrix;
    SimADC::smart_pointer_type adcSim;

    AtomicBoolean stopped;

    ADCAction() {}

    virtual void run()
    {
        while (!stopped.get())
        {
            if (adcSim->updated.wait(1.0))
            {

                try {

                    epicsGuard<epicsMutex> guard(adcSim->mutex);

                    epicsUInt32 len = adcSim->prev_nSamples;
                    double *val = adcSim->data.value.get();
                    static_pointer_cast<PVDoubleArray>(adcMatrix->getScalarArrayField("value", pvDouble))->put(0, len, val, 0);

                    baseValue::shape_t* shape = &adcSim->data.shape;
                    size_t shapeLen = shape->size();
                    vector<int> intVal(shapeLen);
                    for (size_t i = 0; i < shapeLen; i++)
                        intVal[i] = (*shape)[i];
                    static_pointer_cast<PVIntArray>(adcMatrix->getScalarArrayField("dim", pvInt))->put(0, shapeLen, &intVal[0], 0);

                    PVStructure::shared_pointer ts = adcMatrix->getStructureField("timeStamp");

                    PVTimeStamp timeStamp;
                    timeStamp.attach(ts);
                    TimeStamp current;
                    current.put(adcSim->X.timeStamp.tv_sec, adcSim->X.timeStamp.tv_nsec);
                    timeStamp.set(current);

                    notifyStructureChanged(name);

                } catch (std::exception &ex) {
                    std::cerr << "Unhandled exception caught in ADCThread::run(): " << ex.what() << std::endl;
                } catch (...) {
                    std::cerr << "Unhandled exception caught in ADCThread::run()" << std::endl;
                }

            }
        }
    }
};


// testNTImage
class NTImageAction : public Runnable {
public:
    String name;
    PVStructure::shared_pointer pvImage;
    float angle;
    double period;

    AtomicBoolean stopped;

    NTImageAction(double periodHz) :
        angle(0),
        period(periodHz)
    {
    }

    virtual void run()
    {
        while (!stopped.get())
        {

            {
                try {
                    // TODO not nice, since we supply original here
                    rotateImage(pvImage, epicsv4_raw, angle);
                    angle += 1;
                    notifyStructureChanged(name);
                } catch (std::exception &ex) {
                    std::cerr << "Unhandled exception caught in NTImageAction::run(): " << ex.what() << std::endl;
                } catch (...) {
                    std::cerr << "Unhandled exception caught in NTImageAction::run()" << std::endl;
                }

                epicsThreadSleep(period);
            }
        }
    }
};



static epics::pvData::PVStructure::shared_pointer createNTTable(int columnsCount)
{
    StringArray fieldNames(columnsCount);
    FieldConstPtrArray fields(columnsCount);
    char sbuf[16];
    vector<String> labels;
    for (int i = 0; i < columnsCount; i++)
    {
        sprintf(sbuf, "column%d", i);
        fieldNames[i] = sbuf;
        fields[i] = getFieldCreate()->createScalarArray(pvDouble);
        labels.push_back(sbuf);
    }

    Structure::const_shared_pointer valueStructure(
                getFieldCreate()->createStructure(fieldNames, fields)
                );

    StringArray tableFieldNames(2);
    FieldConstPtrArray tableFields(2);
    tableFieldNames[0] = "labels";
    tableFields[0] = getFieldCreate()->createScalarArray(pvString);
    tableFieldNames[1] = "value";
    tableFields[1] = valueStructure;

    PVStructure::shared_pointer result(
                getPVDataCreate()->createPVStructure(
                    getFieldCreate()->createStructure(
                        "uri:ev4:nt/2012/pwd:NTTable", tableFieldNames, tableFields)
                    )
                );
    static_pointer_cast<PVStringArray>(result->getScalarArrayField("labels", pvString))->put(0, labels.size(), &labels[0], 0);

    return result;
}

static void generateNTTableDoubleValues(epics::pvData::PVStructure::shared_pointer result)
{
    PVStringArray::shared_pointer pvLabels(static_pointer_cast<PVStringArray>(result->getScalarArrayField("labels", pvString)));
    StringArrayData ld;
    pvLabels->get(0, pvLabels->getLength(), ld);


    PVStructure::shared_pointer resultValue = result->getStructureField("value");

#define ROWS 10
    double values[ROWS];
#define FILL_VALUES(OFFSET) \
    for (int r = 0; r < ROWS; r++) \
    values[r] = rand()/((double)RAND_MAX+1) + OFFSET;

    int offset = 0;
    for (vector<String>::iterator iter = ld.data.begin();
         iter != ld.data.end();
         iter++, offset++)
    {
        FILL_VALUES(offset);
        static_pointer_cast<PVDoubleArray>(resultValue->getScalarArrayField(*iter, pvDouble))->put(0, ROWS, values, 0);
    }
}


class ChannelFindRequesterImpl : public ChannelFindRequester
{
    virtual void channelFindResult(epics::pvData::Status const & status,
                                   ChannelFind::shared_pointer const & /*channelFind*/, bool wasFound)
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

    virtual void channelCreated(epics::pvData::Status const & /*status*/, Channel::shared_pointer const & /*channel*/)
    {
        //std::cout << "channelCreated(" << status.toString() << ", "
        //          << (channel ? channel->getChannelName() : "(null)") << ")" << std::endl;
    }

    virtual void channelStateChange(Channel::shared_pointer const & /*channel*/, Channel::ConnectionState /*connectionState*/)
    {
        //std::cout << "channelStateChange(" << channel->getChannelName() << ", " << Channel::ConnectionStateNames[connectionState] << ")" << std::endl;
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

    virtual void getDone(const epics::pvData::Status& status,epics::pvData::FieldConstPtr field)
    {
        std::cout << "getDone(" << status.toString() << ", ";
        if (field)
        {
            String str;
            field->toString(&str);
            std::cout << str;
        }
        else
            std::cout << "(null)";
        std::cout << ")" << std::endl;
    }
};

class ChannelGetRequesterImpl : public ChannelGetRequester
{
    //TODO weak ChannelGet::shared_pointer m_channelGet;
    epics::pvData::PVStructure::shared_pointer m_pvStructure;
    epics::pvData::BitSet::shared_pointer m_bitSet;

    virtual String getRequesterName()
    {
        return "ChannelGetRequesterImpl";
    };

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelGetConnect(const epics::pvData::Status& status,
                                   ChannelGet::shared_pointer const & /*channelGet*/,
                                   epics::pvData::PVStructure::shared_pointer const & pvStructure,
                                   epics::pvData::BitSet::shared_pointer const & bitSet)
    {
        std::cout << "channelGetConnect(" << status.toString() << ")" << std::endl;

        //m_channelGet = channelGet;
        m_pvStructure = pvStructure;
        m_bitSet = bitSet;
    }

    virtual void getDone(const epics::pvData::Status& status)
    {
        std::cout << "getDone(" << status.toString() << ")" << std::endl;
        String str;
        m_pvStructure->toString(&str);
        std::cout << str;
        std::cout << std::endl;
    }
};

class ChannelPutRequesterImpl : public ChannelPutRequester
{
    //TODO weak ChannelPut::shared_pointer m_channelPut;
    epics::pvData::PVStructure::shared_pointer m_pvStructure;
    epics::pvData::BitSet::shared_pointer m_bitSet;

    virtual String getRequesterName()
    {
        return "ChannelPutRequesterImpl";
    };

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelPutConnect(const epics::pvData::Status& status,
                                   ChannelPut::shared_pointer const & /*channelPut*/,
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
        String str;
        m_pvStructure->toString(&str);
        std::cout << str;
        std::cout << std::endl;
    }

    virtual void putDone(const epics::pvData::Status& status)
    {
        std::cout << "putDone(" << status.toString() << ")" << std::endl;
        String str;
        m_pvStructure->toString(&str);
        std::cout << str;
        std::cout << std::endl;
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

    virtual void monitorConnect(const Status& status, Monitor::shared_pointer const & /*monitor*/,
                                StructureConstPtr& structure)
    {
        std::cout << "monitorConnect(" << status.toString() << ")" << std::endl;
        if (structure)
        {
            String str;
            structure->toString(&str);
            std::cout << str << std::endl;
        }
    }

    virtual void monitorEvent(Monitor::shared_pointer const & monitor)
    {
        std::cout << "monitorEvent" << std::endl;

        MonitorElement::shared_pointer  element = monitor->poll();

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
    //TODO weak ChannelProcess::shared_pointer m_channelProcess;

    virtual String getRequesterName()
    {
        return "ProcessRequesterImpl";
    };

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelProcessConnect(const epics::pvData::Status& /*status*/,
                                       ChannelProcess::shared_pointer const & /*channelProcess*/)
    {
        //std::cout << "channelProcessConnect(" << status.toString() << ")" << std::endl;

        //m_channelProcess = channelProcess;
    }

    virtual void processDone(const epics::pvData::Status& /*status*/)
    {
        //std::cout << "processDone(" << status.toString() << ")" << std::endl;
    }

};

PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelProcess);

class MockChannelProcess :
        public ChannelProcess,
        public std::tr1::enable_shared_from_this<MockChannelProcess>
{
private:
    String m_channelName;
    ChannelProcessRequester::shared_pointer m_channelProcessRequester;
    PVStructure::shared_pointer m_pvStructure;
    PVScalarPtr m_valueField;
    PVTimeStamp m_timeStamp;

protected:
    MockChannelProcess(String const & channelName, ChannelProcessRequester::shared_pointer const & channelProcessRequester,
                       PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & /*pvRequest*/) :
        m_channelName(channelName), m_channelProcessRequester(channelProcessRequester), m_pvStructure(pvStructure)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelProcess);

        ChannelProcess::shared_pointer thisPtr; // we return null  = static_pointer_cast<ChannelProcess>(shared_from_this());
        PVFieldPtr field = pvStructure->getSubField("value");
        if (field.get() == 0)
        {
            Status noValueFieldStatus(Status::STATUSTYPE_ERROR, "no 'value' field");
            m_channelProcessRequester->channelProcessConnect(noValueFieldStatus, thisPtr);
            // NOTE client must destroy this instance...
            // do not access any fields and return ASAP
            return;
        }

        if (field->getField()->getType() != scalar)
        {
            Status notAScalarStatus(Status::STATUSTYPE_ERROR, "'value' field not scalar type");
            m_channelProcessRequester->channelProcessConnect(notAScalarStatus, thisPtr);
            // NOTE client must destroy this instance...
            // do not access any fields and return ASAP
            return;
        }

        m_valueField = dynamic_pointer_cast<PVScalar>(field);

        PVFieldPtr ts = pvStructure->getSubField("timeStamp");
        if (ts) m_timeStamp.attach(ts);
    }

public:
    static ChannelProcess::shared_pointer create(
            String const & channelName,
            ChannelProcessRequester::shared_pointer const & channelProcessRequester,
            PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        ChannelProcess::shared_pointer thisPtr(new MockChannelProcess(channelName, channelProcessRequester, pvStructure, pvRequest));

        // TODO pvRequest
        channelProcessRequester->channelProcessConnect(Status::Ok, thisPtr);

        return thisPtr;
    }

    virtual ~MockChannelProcess()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannelProcess);
    }


    virtual void process(bool lastRequest)
    {
        if (m_pvStructure->getStructure()->getID() == "uri:ev4:nt/2012/pwd:NTTable")
        {
            generateNTTableDoubleValues(m_pvStructure);
        }
        else if (m_valueField.get())
        {
            switch (m_valueField->getScalar()->getScalarType())
            {
            case pvBoolean:
            {
                // negate
                PVBooleanPtr pvBoolean = static_pointer_cast<PVBoolean>(m_valueField);
                pvBoolean->put(!pvBoolean->get());
                break;
            }
            case pvByte:
            {
                // increment by one
                PVBytePtr pvByte = static_pointer_cast<PVByte>(m_valueField);
                pvByte->put(pvByte->get() + 1);
                break;
            }
            case pvShort:
            {
                // increment by one
                PVShortPtr pvShort = static_pointer_cast<PVShort>(m_valueField);
                pvShort->put(pvShort->get() + 1);
                break;
            }
            case pvInt:
            {
                // increment by one
                PVIntPtr pvInt = static_pointer_cast<PVInt>(m_valueField);
                pvInt->put(pvInt->get() + 1);
                break;
            }
            case pvLong:
            {
                // increment by one
                PVLongPtr pvLong = static_pointer_cast<PVLong>(m_valueField);
                pvLong->put(pvLong->get() + 1);
                break;
            }
            case pvFloat:
            {
                // increment by one
                PVFloatPtr pvFloat = static_pointer_cast<PVFloat>(m_valueField);
                pvFloat->put(pvFloat->get() + 1.0f);
                break;
            }
            case pvDouble:
            {
                double noise = ((rand()/(double)RAND_MAX)-0.5)*2;
                // increment by one
                PVDoublePtr pvDouble = static_pointer_cast<PVDouble>(m_valueField);
                pvDouble->put(pvDouble->get() + noise /*1.0*/);
                break;
            }
            case pvString:
            {
                // increment by one
                PVStringPtr pvString = static_pointer_cast<PVString>(m_valueField);
                String val = pvString->get();
                if (val.empty())
                    pvString->put("gen0");
                else
                {
                    char c = val[3];
                    c++;
                    pvString->put("gen" + c);
                }
                break;
            }
            default:
                // noop
                break;
            }
        }

        if (m_timeStamp.isAttached())
        {
            TimeStamp current;
            current.getCurrent();
            m_timeStamp.set(current);
        }

        m_channelProcessRequester->processDone(Status::Ok);

        notifyStructureChanged(m_channelName);

        if (lastRequest)
            destroy();
    }

    virtual void destroy()
    {
    }
    
    virtual void lock()
    {
        // TODO !!!
    }

    virtual void unlock()
    {
        // TODO !!!
    }
};






PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelGet);

class MockChannelGet : public ChannelGet
{
private:
    ChannelGetRequester::shared_pointer m_channelGetRequester;
    PVStructure::shared_pointer m_pvStructure;
    BitSet::shared_pointer m_bitSet;
    bool m_first;

protected:
    MockChannelGet(ChannelGetRequester::shared_pointer const & channelGetRequester,
                   PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest) :
        m_channelGetRequester(channelGetRequester), m_pvStructure(getRequestedStructure(pvStructure, pvRequest)),
        m_bitSet(new BitSet(m_pvStructure->getNumberFields())), m_first(true)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelGet);
    }

public:
    static ChannelGet::shared_pointer create(ChannelGetRequester::shared_pointer const & channelGetRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        ChannelGet::shared_pointer thisPtr(new MockChannelGet(channelGetRequester, pvStructure, pvRequest));
        channelGetRequester->channelGetConnect(Status::Ok, thisPtr,
                                               static_cast<MockChannelGet*>(thisPtr.get())->m_pvStructure,
                                               static_cast<MockChannelGet*>(thisPtr.get())->m_bitSet);
        return thisPtr;
    }

    virtual ~MockChannelGet()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannelGet);
    }

    virtual void get(bool lastRequest)
    {
        m_channelGetRequester->getDone(Status::Ok);
        if (m_first)
        {
            m_first = false;
            m_bitSet->set(0);  // TODO
        }

        if (lastRequest)
            destroy();
    }

    virtual void destroy()
    {
    }

    virtual void lock()
    {
        // TODO !!!
    }

    virtual void unlock()
    {
        // TODO !!!
    }
};




PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelPut);

class MockChannelPut : public ChannelPut
{
private:
    String m_channelName;
    ChannelPutRequester::shared_pointer m_channelPutRequester;
    PVStructure::shared_pointer m_pvStructure;
    BitSet::shared_pointer m_bitSet;

protected:
    MockChannelPut(String const & channelName, ChannelPutRequester::shared_pointer const & channelPutRequester,
                   PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest) :
        m_channelName(channelName),
        m_channelPutRequester(channelPutRequester), m_pvStructure(getRequestedStructure(pvStructure, pvRequest)),
        m_bitSet(new BitSet(m_pvStructure->getNumberFields()))
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelPut);
    }

public:
    static ChannelPut::shared_pointer create(
            String const & channelName,
            ChannelPutRequester::shared_pointer const & channelPutRequester,
            PVStructure::shared_pointer const & pvStructure,
            PVStructure::shared_pointer const & pvRequest)
    {
        ChannelPut::shared_pointer thisPtr(new MockChannelPut(channelName, channelPutRequester, pvStructure, pvRequest));
        channelPutRequester->channelPutConnect(Status::Ok, thisPtr,
                                               static_cast<MockChannelPut*>(thisPtr.get())->m_pvStructure,
                                               static_cast<MockChannelPut*>(thisPtr.get())->m_bitSet);

        return thisPtr;
    }

    virtual ~MockChannelPut()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannelPut);
    }


    virtual void put(bool lastRequest)
    {
        m_channelPutRequester->putDone(Status::Ok);

        notifyStructureChanged(m_channelName);

        if (lastRequest)
            destroy();
    }

    virtual void get()
    {
        m_channelPutRequester->getDone(Status::Ok);
    }

    virtual void destroy()
    {
    }

    virtual void lock()
    {
        // TODO !!!
    }

    virtual void unlock()
    {
        // TODO !!!
    }
};




PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelPutGet);

class MockChannelPutGet : public ChannelPutGet
{
private:
    String m_channelName;
    ChannelPutGetRequester::shared_pointer m_channelPutGetRequester;
    PVStructure::shared_pointer m_getStructure;
    PVStructure::shared_pointer m_putStructure;

protected:
    MockChannelPutGet(String const & channelName, ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
                      PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest) :
        m_channelName(channelName),
        m_channelPutGetRequester(channelPutGetRequester),
        m_getStructure(getRequestedStructure(pvStructure, pvRequest, "getField")),
        m_putStructure(getRequestedStructure(pvStructure, pvRequest, "putField"))
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelPutGet);
    }

public:
    static ChannelPutGet::shared_pointer create(
            String const & channelName,
            ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
            PVStructure::shared_pointer const & pvStructure,
            PVStructure::shared_pointer const & pvRequest)
    {
        ChannelPutGet::shared_pointer thisPtr(new MockChannelPutGet(channelName, channelPutGetRequester, pvStructure, pvRequest));

        channelPutGetRequester->channelPutGetConnect(Status::Ok, thisPtr,
                                                     static_cast<MockChannelPutGet*>(thisPtr.get())->m_putStructure,
                                                     static_cast<MockChannelPutGet*>(thisPtr.get())->m_getStructure);

        return thisPtr;
    }

    virtual ~MockChannelPutGet()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannelPutGet);
    }

    virtual void putGet(bool lastRequest)
    {
        m_channelPutGetRequester->putGetDone(Status::Ok);

        notifyStructureChanged(m_channelName);

        if (lastRequest)
            destroy();
    }

    virtual void getGet()
    {
        m_channelPutGetRequester->getGetDone(Status::Ok);
    }

    virtual void getPut()
    {
        m_channelPutGetRequester->getPutDone(Status::Ok);
    }

    virtual void destroy()
    {
    }

    virtual void lock()
    {
        // TODO !!!
    }

    virtual void unlock()
    {
        // TODO !!!
    }
};



static bool handleHelp(
        epics::pvData::PVStructure::shared_pointer const & args,
        ChannelRPCRequester::shared_pointer const & channelRPCRequester,
        String const & helpText
        )
{
    if (args->getSubField("help"))
    {

        StringArray fieldNames(1);
        FieldConstPtrArray fields(1);
        fieldNames[0] = "value";
        fields[0] = getFieldCreate()->createScalar(pvString);

        PVStructure::shared_pointer result(
                    getPVDataCreate()->createPVStructure(
                        getFieldCreate()->createStructure(
                            "uri:ev4:nt/2012/pwd:NTScalar", fieldNames, fields)
                        )
                    );

        static_pointer_cast<PVString>(result->getStringField("value"))->put(helpText);
        channelRPCRequester->requestDone(Status::Ok, result);
        return true;
    }
    else
    {
        return false;
    }
}


PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelRPC);

class MockChannelRPC : public ChannelRPC
{
private:
    ChannelRPCRequester::shared_pointer m_channelRPCRequester;
    String m_channelName;
    PVStructure::shared_pointer m_pvStructure;

protected:
    MockChannelRPC(ChannelRPCRequester::shared_pointer const & channelRPCRequester,
                   String const & channelName, PVStructure::shared_pointer const & pvStructure,
                   PVStructure::shared_pointer const & /*pvRequest*/) :
        m_channelRPCRequester(channelRPCRequester), m_channelName(channelName), m_pvStructure(pvStructure)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelRPC);
    }

public:
    static ChannelRPC::shared_pointer create(ChannelRPCRequester::shared_pointer const & channelRPCRequester, String const & channelName, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        ChannelRPC::shared_pointer thisPtr(new MockChannelRPC(channelRPCRequester, channelName, pvStructure, pvRequest));
        // TODO pvRequest
        channelRPCRequester->channelRPCConnect(Status::Ok, thisPtr);
        return thisPtr;
    }

    virtual ~MockChannelRPC()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannelRPC);
    }

    virtual void request(epics::pvData::PVStructure::shared_pointer const & pvArgument, bool lastRequest)
    {
        if (m_channelName == "testNTTable")
        {
            PVStructure::shared_pointer args(
                        (pvArgument->getStructure()->getID() == "uri:ev4:nt/2012/pwd:NTURI") ?
                            pvArgument->getStructureField("query") :
                            pvArgument
                            );

            const String helpText =
                    "Generates a NTTable structure response with 10 rows and a specified number of columns.\n"
                    "Columns are labeled 'column<num>' and values are '<num> + random [0..1)'.\n"
                    "Arguments:\n\tstring columns\tnumber of table columns\n";
            if (handleHelp(args, m_channelRPCRequester, helpText))
                return;

            PVStringPtr columns = dynamic_pointer_cast<PVString>(args->getSubField("columns"));
            if (columns.get() == 0)
            {
                PVStructure::shared_pointer nullPtr;
                Status errorStatus(Status::STATUSTYPE_ERROR, "no string 'columns' argument specified");
                m_channelRPCRequester->requestDone(errorStatus, nullPtr);
            }
            else
            {
                int columnsCount = atoi(columns->get().c_str());
                PVStructure::shared_pointer result = createNTTable(columnsCount);
                generateNTTableDoubleValues(result);
                m_channelRPCRequester->requestDone(Status::Ok, result);
            }
        }
        else if (m_channelName == "testNTNameValue")
        {
            PVStructure::shared_pointer args(
                        (pvArgument->getStructure()->getID() == "uri:ev4:nt/2012/pwd:NTURI") ?
                            pvArgument->getStructureField("query") :
                            pvArgument
                            );

            const String helpText =
                    "Generates a NTNameValue structure response with a specified number of columns.\n"
                    "Columns are labeled 'name<num>' and values are '<num> + random [0..1)'.\n"
                    "Arguments:\n\tstring columns\tnumber of columns\n";
            if (handleHelp(args, m_channelRPCRequester, helpText))
                return;

            PVStringPtr columns = dynamic_pointer_cast<PVString>(args->getSubField("columns"));
            if (columns.get() == 0)
            {
                PVStructure::shared_pointer nullPtr;
                Status errorStatus(Status::STATUSTYPE_ERROR, "no string 'columns' argument specified");
                m_channelRPCRequester->requestDone(errorStatus, nullPtr);
            }
            else
            {

                int columnsCount = atoi(columns->get().c_str());

                char sbuf[16];
                vector<String> labels;
                for (int i = 0; i < columnsCount; i++)
                {
                    sprintf(sbuf, "name%d", i);
                    labels.push_back(sbuf);
                }

                StringArray tableFieldNames(2);
                FieldConstPtrArray tableFields(2);
                tableFieldNames[0] = "name";
                tableFields[0] = getFieldCreate()->createScalarArray(pvString);
                tableFieldNames[1] = "value";
                tableFields[1] = getFieldCreate()->createScalarArray(pvDouble);

                PVStructure::shared_pointer result(
                            getPVDataCreate()->createPVStructure(
                                getFieldCreate()->createStructure(
                                    "uri:ev4:nt/2012/pwd:NTNameValue", tableFieldNames, tableFields)
                                )
                            );
                static_pointer_cast<PVStringArray>(result->getScalarArrayField("name", pvString))->put(0, labels.size(), &labels[0], 0);

                int32 len = columnsCount;
                vector<double> mv(len);
                for (int r = 0; r < len; r++)
                    mv[r] = rand()/((double)RAND_MAX+1) + (int)(r);
                static_pointer_cast<PVDoubleArray>(result->getScalarArrayField("value", pvDouble))->put(0, len, &mv[0], 0);

                m_channelRPCRequester->requestDone(Status::Ok, result);
            }
        }
        else if (m_channelName == "testNTMatrix")
        {
            PVStructure::shared_pointer args(
                        (pvArgument->getStructure()->getID() == "uri:ev4:nt/2012/pwd:NTURI") ?
                            pvArgument->getStructureField("query") :
                            pvArgument
                            );

            const String helpText =
                    "Generates a NTMatrix structure response with a specified number of rows and columns.\n"
                    "Matrix values are '<row> + random [0..1)'.\n"
                    "Arguments:\n"
                    "\tstring rows\tnumber of matrix rows\n"
                    "\tstring columns\tnumber of matrix columns\n"
                    "\t[string bycolumns\torder matrix values in a column-major order]\n";
            if (handleHelp(args, m_channelRPCRequester, helpText))
                return;

            PVStringPtr rows = dynamic_pointer_cast<PVString>(args->getSubField("rows"));
            PVStringPtr columns = dynamic_pointer_cast<PVString>(args->getSubField("columns"));
            if (rows.get() == 0 || columns.get() == 0)
            {
                PVStructure::shared_pointer nullPtr;
                Status errorStatus(Status::STATUSTYPE_ERROR, "no string 'rows' and 'columns' arguments specified");
                m_channelRPCRequester->requestDone(errorStatus, nullPtr);
            }
            else
            {
                int i = 0;
                int totalFields = 2;  // value[], dim
                StringArray fieldNames(totalFields);
                FieldConstPtrArray fields(totalFields);
                fieldNames[i] = "value";
                fields[i++] = getFieldCreate()->createScalarArray(pvDouble);
                fieldNames[i] = "dim";
                fields[i++] = getFieldCreate()->createScalarArray(pvInt);

                PVStructure::shared_pointer result(
                            getPVDataCreate()->createPVStructure(
                                getFieldCreate()->createStructure("uri:ev4:nt/2012/pwd:NTMatrix", fieldNames, fields)
                                )
                            );

                int32 rowsVal = atoi(rows->get().c_str());
                int32 colsVal = atoi(columns->get().c_str());
                int32 dimValues[] = { rowsVal, colsVal };
                static_pointer_cast<PVIntArray>(result->getScalarArrayField("dim", pvInt))->put(0, 2, &dimValues[0], 0);

                PVStringPtr byColumns = static_pointer_cast<PVString>(args->getSubField("bycolumns"));
                bool bycolumns = (byColumns.get() && byColumns->get() == "1");

                int32 len = rowsVal * colsVal;
                vector<double> mv(len);
                for (int r = 0; r < len; r++)
                    if (bycolumns)
                        mv[r] = rand()/((double)RAND_MAX+1) + (int)(r%rowsVal);
                    else
                        mv[r] = rand()/((double)RAND_MAX+1) + (int)(r/colsVal);
                static_pointer_cast<PVDoubleArray>(result->getScalarArrayField("value", pvDouble))->put(0, len, &mv[0], 0);

                m_channelRPCRequester->requestDone(Status::Ok, result);
            }
        }
        else if (m_channelName.find("testImage") == 0)
        {
            PVStructure::shared_pointer args(
                        (pvArgument->getStructure()->getID() == "uri:ev4:nt/2012/pwd:NTURI") ?
                            pvArgument->getStructureField("query") :
                            pvArgument
                            );

            const String helpText =
                    "Generates a NTImage structure response that has encoded a specified image.\n"
                    "Arguments:\n"
                    "\tstring file\tfile path (relative to a location where the server was started) of a raw encoded image.\n"
                    "\t\t\tTwo image types are supported:\n"
                    "\t\t\t\t- RGB888 encoded (file extension '.rgb')\n"
                    "\t\t\t\t- 8-bit grayscale encoded (any other extension).\n"
                    "\t\t\tTo generate such encoded images you can use ImageMagick 'convert' tool, e.g.:\n"
                    "\t\t\t\tconvert my_image.png my_image.rgb\n"
                    "\tstring w\timage width\n"
                    "\tstring h\timage height\n";
            if (handleHelp(args, m_channelRPCRequester, helpText))
                return;

            PVStringPtr file = dynamic_pointer_cast<PVString>(args->getSubField("file"));
            PVStringPtr w = dynamic_pointer_cast<PVString>(args->getSubField("w"));
            PVStringPtr h = dynamic_pointer_cast<PVString>(args->getSubField("h"));
            if (file.get() == 0 || w.get() == 0 || h.get() == 0)
            {
                PVStructure::shared_pointer nullPtr;
                Status errorStatus(Status::STATUSTYPE_ERROR, "not all 'file', 'w' and 'h' arguments specified");
                m_channelRPCRequester->requestDone(errorStatus, nullPtr);
            }
            else
            {
                int32 wv = atoi(w->get().c_str());
                int32 hv = atoi(h->get().c_str());
                String filev = file->get();

                // ImageMagick conversion
                // RGB888:    convert img.png img.rgb
                // grayscale: convert img.png img.g
                bool isRGB = (filev.find(".rgb") != string::npos);

                ifstream in(filev.c_str(), ifstream::in | ifstream::binary);
                if (in.is_open())
                {
                    // get file size
                    in.seekg(0, ifstream::end);
                    std::size_t fileSize = in.tellg();

                    // in case of negative values, etc., this will return right result, however it will fail next check
                    std::size_t expectedSize = wv*hv* (isRGB ? 3 : 1);
                    if (expectedSize == fileSize)
                    {
                        in.seekg(0, ifstream::beg);

                        // TODO sync missing on m_pvStructure
                        if (isRGB)
                        {
                            const int32_t dim[] = { 3, wv, hv };
                            initImage(m_pvStructure, 2 /* RGB */, 3, dim, fileSize, 0);
                        }
                        else
                        {
                            const int32_t dim[] = { wv, hv };
                            initImage(m_pvStructure, 0 /* grayscale */, 2, dim, fileSize, 0);
                        }

                        PVByteArrayPtr value = std::tr1::dynamic_pointer_cast<PVByteArray>(m_pvStructure->getSubField("value"));
                        value->setCapacity(fileSize);

                        value->setLength(fileSize);
                        in.readsome((char*)value->get(), fileSize);

                        m_channelRPCRequester->requestDone(Status::Ok, m_pvStructure);

                        // for monitors
                        notifyStructureChanged(m_channelName);
                    }
                    else
                    {
                        PVStructure::shared_pointer nullPtr;
                        Status errorStatus(Status::STATUSTYPE_ERROR, "file size does not match given 'w' and 'h'");
                        m_channelRPCRequester->requestDone(errorStatus, nullPtr);
                    }
                }
                else
                {
                    PVStructure::shared_pointer nullPtr;
                    Status errorStatus(Status::STATUSTYPE_ERROR, "failed to open image file specified");
                    m_channelRPCRequester->requestDone(errorStatus, nullPtr);
                }
            }
        }
        else if (m_channelName == "testNTURI")
        {
            const String helpText =
                    "Returns the NTURI structure response identical the NTURI request.\n"
                    "Arguments: (none)\n";
            if (handleHelp(pvArgument, m_channelRPCRequester, helpText))
                return;

            if (pvArgument->getStructure()->getID() != "uri:ev4:nt/2012/pwd:NTURI")
            {
                PVStructure::shared_pointer nullPtr;
                Status errorStatus(Status::STATUSTYPE_ERROR, "argument is not a NTURI structure");
                m_channelRPCRequester->requestDone(errorStatus, nullPtr);
            }
            else
            {
                // return argument as result
                m_channelRPCRequester->requestDone(Status::Ok, pvArgument);
            }
        }
        else if (m_channelName.find("testServerShutdown") == 0)
        {
            PVStructure::shared_pointer nullPtr;
            m_channelRPCRequester->requestDone(Status::Ok, nullPtr);
            testServerShutdown();
        }
        else
        {
            /*
            std::string s;
            pvArgument->toString(&s);
            std::cout << "RPC" << std::endl << s << std::endl;
            */
            m_channelRPCRequester->requestDone(Status::Ok, m_pvStructure);
        }

        if (lastRequest)
            destroy();
    }

    virtual void destroy()
    {
    }

    virtual void lock()
    {
        // TODO !!!
    }

    virtual void unlock()
    {
        // TODO !!!
    }
};










PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelArray);

class MockChannelArray : public ChannelArray
{
private:
    ChannelArrayRequester::shared_pointer m_channelArrayRequester;
    PVArray::shared_pointer m_pvArray;

protected:
    MockChannelArray(ChannelArrayRequester::shared_pointer const & channelArrayRequester,
                     PVStructure::shared_pointer const & /*pvStructure*/, PVStructure::shared_pointer const & /*pvRequest*/) :
        m_channelArrayRequester(channelArrayRequester),
        m_pvArray(getPVDataCreate()->createPVScalarArray(pvDouble))
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelArray);
    }

public:
    static ChannelArray::shared_pointer create(ChannelArrayRequester::shared_pointer const & channelArrayRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        ChannelArray::shared_pointer thisPtr(new MockChannelArray(channelArrayRequester, pvStructure, pvRequest));

        // TODO pvRequest
        channelArrayRequester->channelArrayConnect(Status::Ok, thisPtr, static_cast<MockChannelArray*>(thisPtr.get())->m_pvArray);

        return thisPtr;
    }

    virtual ~MockChannelArray()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannelArray);
    }

    virtual void putArray(bool lastRequest, int /*offset*/, int /*count*/)
    {
        // TODO offset, count
        m_channelArrayRequester->putArrayDone(Status::Ok);
        if (lastRequest)
            destroy();
    }

    virtual void getArray(bool lastRequest, int /*offset*/, int /*count*/)
    {
        // TODO offset, count
        m_channelArrayRequester->getArrayDone(Status::Ok);
        if (lastRequest)
            destroy();
    }

    virtual void setLength(bool lastRequest, int /*length*/, int /*capacity*/)
    {
        // TODO offset, capacity
        m_channelArrayRequester->setLengthDone(Status::Ok);
        if (lastRequest)
            destroy();
    }

    virtual void destroy()
    {
    }

    virtual void lock()
    {
        // TODO !!!
    }

    virtual void unlock()
    {
        // TODO !!!
    }
};






PVACCESS_REFCOUNT_MONITOR_DEFINE(mockMonitor);

class MockMonitor : public Monitor, public StructureChangedCallback, public std::tr1::enable_shared_from_this<MockMonitor>
{
private:
    String m_channelName;
    MonitorRequester::shared_pointer m_monitorRequester;
    PVStructure::shared_pointer m_pvStructure;
    BitSet::shared_pointer m_changedBitSet;
    BitSet::shared_pointer m_overrunBitSet;
    bool m_first;
    Mutex m_lock;
    int m_count;

    MonitorElement::shared_pointer m_thisPtr;
    MonitorElement::shared_pointer m_nullMonitor;

protected:
    MockMonitor(String const & channelName, MonitorRequester::shared_pointer const & monitorRequester,
                PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest) :
        m_channelName(channelName),
        m_monitorRequester(monitorRequester), m_pvStructure(getRequestedStructure(pvStructure, pvRequest)),
        m_changedBitSet(new BitSet(m_pvStructure->getNumberFields())),
        m_overrunBitSet(new BitSet(m_pvStructure->getNumberFields())),
        m_first(true),
        m_lock(),
        m_count(0),
        m_thisPtr(new MonitorElement())
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockMonitor);

        m_changedBitSet->set(0);

        m_thisPtr->pvStructurePtr = m_pvStructure;
        m_thisPtr->changedBitSet = m_changedBitSet;
        m_thisPtr->overrunBitSet = m_overrunBitSet;
    }

public:
    static Monitor::shared_pointer create(String const & channelName,
                                          MonitorRequester::shared_pointer const & monitorRequester,
                                          PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        Monitor::shared_pointer thisPtr(new MockMonitor(channelName, monitorRequester, pvStructure, pvRequest));
        StructureConstPtr structurePtr = static_cast<MockMonitor*>(thisPtr.get())->m_pvStructure->getStructure();
        monitorRequester->monitorConnect(Status::Ok, thisPtr, structurePtr);

        structureChangedListeners[channelName].push_back(std::tr1::dynamic_pointer_cast<StructureChangedCallback>(thisPtr));
        return thisPtr;
    }

    virtual ~MockMonitor()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockMonitor);
    }

    virtual Status start()
    {
        // first monitor
        Monitor::shared_pointer thisPtr = shared_from_this();
        m_monitorRequester->monitorEvent(thisPtr);

        return Status::Ok;
    }

    virtual Status stop()
    {
        return Status::Ok;
    }

    virtual void structureChanged()
    {
        m_count = 0;
        Monitor::shared_pointer thisPtr = shared_from_this();
        m_monitorRequester->monitorEvent(thisPtr);
    }

    virtual MonitorElement::shared_pointer poll()
    {
        Lock xx(m_lock);
        if (m_count)
        {
            return m_nullMonitor;
        }
        else
        {
            m_count++;
            return m_thisPtr;
        }
    }

    virtual void release(MonitorElement::shared_pointer const & /*monitorElement*/)
    {
        Lock xx(m_lock);
        if (m_count)
        {
            m_count--;
        }
    }

    virtual void destroy()
    {
        stop();

        // remove itself from listeners table
        vector<StructureChangedCallback::shared_pointer> &vec = structureChangedListeners[m_channelName];
        vec.erase(find(vec.begin(), vec.end(), std::tr1::dynamic_pointer_cast<StructureChangedCallback>(shared_from_this())));
    }

    virtual void lock()
    {
        // TODO !!!
    }

    virtual void unlock()
    {
        // TODO !!!
    }

};


PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannel);

class MockChannel : public Channel {
private:
    ChannelProvider::weak_pointer  m_provider;
    ChannelRequester::shared_pointer m_requester;
    String m_name;
    String m_remoteAddress;
public: // TODO
    PVStructure::shared_pointer m_pvStructure;

protected:

    MockChannel(
            ChannelProvider::shared_pointer provider,
            ChannelRequester::shared_pointer requester,
            String name,
            String remoteAddress) :
        m_provider(provider),
        m_requester(requester),
        m_name(name),
        m_remoteAddress(remoteAddress),
        m_pvStructure()
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannel);

        if (structureStore.find(m_name) != structureStore.end())
            m_pvStructure = structureStore[m_name];

        else
        {
            // create structure

            if (m_name.find("testArray") == 0)
            {
                String allProperties("");
                //            String allProperties("alarm,timeStamp,display,control");
                m_pvStructure = getStandardPVField()->scalarArray(pvDouble,allProperties);
                PVDoubleArrayPtr pvField = static_pointer_cast<PVDoubleArray>(m_pvStructure->getScalarArrayField(String("value"), pvDouble));

                int specCount = 0; char postfix[64];
                int done = sscanf(m_name.c_str(), "testArray%d%s", &specCount, postfix);

                if (done && specCount > 0)
                {
                    pvField->setCapacity(specCount);
                    pvField->setLength(specCount);

                    double v = 0;
                    int ix = 0;
                    const int COUNT = 1024;

                    int n = 0;
                    while (n < specCount)
                    {

                        double array[COUNT];
                        int i = 0;
                        for (; i < COUNT && n < specCount; i++)
                        {
                            array[i] = v; v+=1; n++;
                        }
                        pvField->put(ix, i, array, 0);
                        ix += i;
                    }
                }
                else
                {
                    double v = 0;
                    int ix = 0;
                    const int COUNT = 1024;

                    pvField->setCapacity(1024*COUNT);
                    for (int n = 0; n < 1024; n++)
                    {

                        double array[COUNT];
                        for (int i = 0; i < COUNT; i++)
                        {
                            array[i] = v; v+=1.1;
                        }
                        pvField->put(ix, COUNT, array, 0);
                        ix += COUNT;
                    }
                }
                /*
                printf("array prepared------------------------------------!!!\n");
                String str;
                pvField->toString(&str);
                printf("%s\n", str.c_str());
                printf("=============------------------------------------!!!\n");
                */
            }
            else if (m_name.find("testMP") == 0
                     || m_name.find("testImage") == 0)
            {
                m_pvStructure = getPVDataCreate()->createPVStructure(makeImageStruc());
                initImageEPICSv4GrayscaleLogo(m_pvStructure);
            }
            else if (m_name.find("testTable") == 0)
            {
                m_pvStructure = createNTTable(5);   // 5 columns
                generateNTTableDoubleValues(m_pvStructure);
            }
            else if (m_name.find("testADC") == 0)
            {
                int i = 0;
                int totalFields = 6;
                StringArray fieldNames(totalFields);
                FieldConstPtrArray fields(totalFields);
                fieldNames[i] = "value";
                fields[i++] = getFieldCreate()->createScalarArray(pvDouble);
                fieldNames[i] = "dim";
                fields[i++] = getFieldCreate()->createScalarArray(pvInt);
                fieldNames[i] = "descriptor";
                fields[i++] = getFieldCreate()->createScalar(pvString);
                fieldNames[i] = "timeStamp";
                fields[i++] = getStandardField()->timeStamp();
                fieldNames[i] = "alarm";
                fields[i++] = getStandardField()->alarm();
                fieldNames[i] = "display";
                fields[i++] = getStandardField()->display();

                m_pvStructure =
                        getPVDataCreate()->createPVStructure(
                            getFieldCreate()->createStructure("uri:ev4:nt/2012/pwd:NTMatrix", fieldNames, fields)
                            );

                // fill with default values
                int dimValue = 0;
                static_pointer_cast<PVIntArray>(m_pvStructure->getScalarArrayField("dim", pvInt))->put(0, 1, &dimValue, 0);

                m_pvStructure->getStringField("descriptor")->put("Simulated ADC that provides NTMatrix value");
                PVStructurePtr displayStructure = m_pvStructure->getStructureField("display");
                displayStructure->getDoubleField("limitLow")->put(-1.0);
                displayStructure->getDoubleField("limitHigh")->put(1.0);
                displayStructure->getStringField("description")->put("Simulated ADC");
                displayStructure->getStringField("format")->put("%f");
                displayStructure->getStringField("units")->put("V");
            }
            else if (m_name.find("testRPC") == 0 || m_name == "testNTTable" || m_name == "testNTMatrix")
            {
                StringArray fieldNames;
                PVFieldPtrArray fields;
                m_pvStructure = getPVDataCreate()->createPVStructure(fieldNames, fields);
            }
            else if (m_name.find("testValueOnly") == 0)
            {
                String allProperties("");
                m_pvStructure = getStandardPVField()->scalar(pvDouble,allProperties);
            }
            else if (m_name == "testCounter")
            {
                String allProperties("timeStamp");
                m_pvStructure = getStandardPVField()->scalar(pvInt,allProperties);
            }
            else
            {
                String allProperties("alarm,timeStamp,display,control,valueAlarm");
                m_pvStructure = getStandardPVField()->scalar(pvDouble,allProperties);
                //PVDoublePtr pvField = m_pvStructure->getDoubleField(String("value"));
                //pvField->put(1.123);
            }

            structureStore[m_name] = m_pvStructure;
        }
    }
    
public:

    static Channel::shared_pointer create(
            ChannelProvider::shared_pointer provider,
            ChannelRequester::shared_pointer requester,
            String name,
            String remoteAddress)
    {
        Channel::shared_pointer channelPtr(new MockChannel(provider, requester, name, remoteAddress));

        // already connected, report state
        requester->channelStateChange(channelPtr, CONNECTED);
        
        return channelPtr;
    }

    virtual ~MockChannel()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannel);
    }

    virtual void destroy()
    {
    };

    virtual String getRequesterName()
    {
        return getChannelName();
    };

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual ChannelProvider::shared_pointer getProvider()
    {
        return m_provider.lock();
    }

    virtual epics::pvData::String getRemoteAddress()
    {
        return m_remoteAddress;
    }

    virtual epics::pvData::String getChannelName()
    {
        return m_name;
    }

    virtual  std::tr1::shared_ptr<ChannelRequester> getChannelRequester()
    {
        return m_requester;
    }

    virtual ConnectionState getConnectionState()
    {
        return CONNECTED;
    }

    virtual bool isConnected()
    {
        return getConnectionState() == CONNECTED;
    }

    virtual AccessRights getAccessRights(epics::pvData::PVField::shared_pointer const & /*pvField*/)
    {
        return readWrite;
    }

    virtual void getField(GetFieldRequester::shared_pointer const & requester,epics::pvData::String const & subField)
    {
        PVFieldPtr pvField;
        if(subField == "")
        {
            pvField = m_pvStructure;
        }
        else
        {
            pvField = m_pvStructure->getSubField(subField);
        }

        if(pvField == NULL)
        {
            string errMsg = "field '" + subField + "' not found";
            FieldConstPtr nullPtr;
            Status errorStatus(Status::STATUSTYPE_ERROR, errMsg);
            requester->getDone(errorStatus,nullPtr);
            return;
        }
        FieldConstPtr fieldPtr = pvField->getField();
        requester->getDone(Status::Ok, fieldPtr);
    }

    virtual ChannelProcess::shared_pointer createChannelProcess(
            ChannelProcessRequester::shared_pointer const & channelProcessRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
        return MockChannelProcess::create(m_name, channelProcessRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelGet::shared_pointer createChannelGet(
            ChannelGetRequester::shared_pointer const & channelGetRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
        return MockChannelGet::create(channelGetRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelPut::shared_pointer createChannelPut(
            ChannelPutRequester::shared_pointer const & channelPutRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
        return MockChannelPut::create(m_name, channelPutRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelPutGet::shared_pointer createChannelPutGet(
            ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
        return MockChannelPutGet::create(m_name, channelPutGetRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelRPC::shared_pointer createChannelRPC(ChannelRPCRequester::shared_pointer const & channelRPCRequester,
                                                        epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
        return MockChannelRPC::create(channelRPCRequester, m_name, m_pvStructure, pvRequest);
    }

    virtual epics::pvData::Monitor::shared_pointer createMonitor(
            epics::pvData::MonitorRequester::shared_pointer const & monitorRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
        return MockMonitor::create(m_name, monitorRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelArray::shared_pointer createChannelArray(
            ChannelArrayRequester::shared_pointer const & channelArrayRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
        return MockChannelArray::create(channelArrayRequester, m_pvStructure, pvRequest);
    }

    virtual void printInfo() {
        String info;
        printInfo(&info);
        std::cout << info.c_str() << std::endl;
    }

    virtual void printInfo(epics::pvData::StringBuilder out) {
        //std::ostringstream ostr;
        //static String emptyString;

        out->append(  "CHANNEL  : "); out->append(m_name);
        out->append("\nSTATE    : "); out->append(ConnectionStateNames[getConnectionState()]);
        if (isConnected())
        {
            out->append("\nADDRESS  : "); out->append(getRemoteAddress());
            //out->append("\nRIGHTS   : "); out->append(getAccessRights());
        }
        out->append("\n");
    }
};


class MockServerChannelProvider;

class MockChannelFind : public ChannelFind
{
public:
    typedef std::tr1::shared_ptr<MockChannelFind> shared_pointer;
    typedef std::tr1::shared_ptr<const MockChannelFind> const_shared_pointer;

    MockChannelFind(ChannelProvider::shared_pointer &provider) : m_provider(provider)
    {
    }

    virtual ~MockChannelFind() {}

    virtual void destroy()
    {
        // one instance for all, do not delete at all
    }

    virtual ChannelProvider::shared_pointer getChannelProvider()
    {
        return m_provider.lock();
    };

    virtual void cancelChannelFind()
    {
        throw std::runtime_error("not supported");
    }

private:
    ChannelProvider::weak_pointer m_provider;
};


class MockServerChannelProvider : 	public ChannelProvider,
        public std::tr1::enable_shared_from_this<MockServerChannelProvider>
{
public:
    typedef std::tr1::shared_ptr<MockServerChannelProvider> shared_pointer;
    typedef std::tr1::shared_ptr<const MockServerChannelProvider> const_shared_pointer;

    static String PROVIDER_NAME;

    MockServerChannelProvider() :
        m_mockChannelFind(),
        m_counterChannel(),
        m_adcChannel(),
        m_mpChannel(),
        m_scan1Hz(1.0),
        m_scan1HzThread(),
        m_scan10Hz(0.1),
        m_scan10HzThread(),
        m_adcAction(),
        m_adcThread(),
        m_imgAction(0.1),
        m_imgThread()
    {
    }

    virtual ~MockServerChannelProvider()
    {
        m_scan1Hz.stopped.set();
        m_scan10Hz.stopped.set();
        m_adcAction.stopped.set();
        m_imgAction.stopped.set();
    }

    void initialize()
    {
        ChannelProvider::shared_pointer chProviderPtr = shared_from_this();
        m_mockChannelFind.reset(new MockChannelFind(chProviderPtr));


        std::tr1::shared_ptr<ChannelRequesterImpl> cr(new ChannelRequesterImpl());
        m_counterChannel = MockChannel::create(chProviderPtr, cr, "testCounter", "local");
        std::tr1::shared_ptr<ChannelProcessRequesterImpl> cpr(new ChannelProcessRequesterImpl());
        ChannelProcess::shared_pointer process = m_counterChannel->createChannelProcess(cpr, PVStructure::shared_pointer());
        m_scan1Hz.toProcess.push_back(process);

        Channel::shared_pointer c = MockChannel::create(chProviderPtr, cr, "testRandom", "local");
        process = c->createChannelProcess(cpr, PVStructure::shared_pointer());
        m_scan10Hz.toProcess.push_back(process);

        c = MockChannel::create(chProviderPtr, cr, "testTable", "local");
        process = c->createChannelProcess(cpr, PVStructure::shared_pointer());
        m_scan1Hz.toProcess.push_back(process);

        m_scan1HzThread.reset(new Thread("process1hz", highPriority, &m_scan1Hz));
        m_scan10HzThread.reset(new Thread("process10hz", highPriority, &m_scan10Hz));

        m_adcChannel = MockChannel::create(chProviderPtr, cr, "testADC", "local");
        m_adcAction.name = "testADC";
        m_adcAction.adcMatrix = static_pointer_cast<MockChannel>(m_adcChannel)->m_pvStructure;
        m_adcAction.adcSim = createSimADC("testADC");
        m_adcThread.reset(new Thread("adcThread", highPriority, &m_adcAction));

        m_mpChannel = MockChannel::create(chProviderPtr, cr, "testMP", "local");
        m_imgAction.name = "testMP";
        m_imgAction.pvImage = static_pointer_cast<MockChannel>(m_mpChannel)->m_pvStructure;
        m_imgThread.reset(new Thread("imgThread", highPriority, &m_imgAction));
    }

    virtual epics::pvData::String getProviderName()
    {
        return PROVIDER_NAME;
    }

    virtual void destroy()
    {
    }

    virtual ChannelFind::shared_pointer channelFind(
            epics::pvData::String const & channelName,
            ChannelFindRequester::shared_pointer const & channelFindRequester)
    {
        // channel that starts with "test" always exists
        bool exists = (channelName.find("test") == 0);
        channelFindRequester->channelFindResult(Status::Ok, m_mockChannelFind, exists);
        return m_mockChannelFind;
    }

    virtual Channel::shared_pointer createChannel(
            epics::pvData::String const & channelName,
            ChannelRequester::shared_pointer const & channelRequester,
            short priority)
    {
        return createChannel(channelName, channelRequester, priority, "local");
    }

    virtual Channel::shared_pointer createChannel(
            epics::pvData::String const & channelName,
            ChannelRequester::shared_pointer const & channelRequester,
            short /*priority*/,
            epics::pvData::String const & address)
    {
        if (address == "local")
        {
            if (channelName == "testCounter")
            {
                channelRequester->channelCreated(Status::Ok, m_counterChannel);
                return m_counterChannel;
            }
            else if (channelName == "testADC")
            {
                channelRequester->channelCreated(Status::Ok, m_adcChannel);
                return m_adcChannel;
            }
            else if (channelName == "testMP")
            {
                channelRequester->channelCreated(Status::Ok, m_mpChannel);
                return m_mpChannel;
            }
            else
            {
                ChannelProvider::shared_pointer chProviderPtr = shared_from_this();
                Channel::shared_pointer channel = MockChannel::create(chProviderPtr, channelRequester, channelName, address);
                channelRequester->channelCreated(Status::Ok, channel);
                return channel;
            }
        }
        else
        {
            Channel::shared_pointer nullPtr;
            Status errorStatus(Status::STATUSTYPE_ERROR, "only local supported");
            channelRequester->channelCreated(errorStatus, nullPtr);
            return nullPtr;
        }
    }
private:

    ChannelFind::shared_pointer m_mockChannelFind;
    Channel::shared_pointer m_counterChannel;
    Channel::shared_pointer m_adcChannel;
    Channel::shared_pointer m_mpChannel;

    ProcessAction m_scan1Hz;
    auto_ptr<Thread> m_scan1HzThread;

    ProcessAction m_scan10Hz;
    auto_ptr<Thread> m_scan10HzThread;

    ADCAction m_adcAction;
    auto_ptr<Thread> m_adcThread;

    NTImageAction m_imgAction;
    auto_ptr<Thread> m_imgThread;
};

String MockServerChannelProvider::PROVIDER_NAME = "local";

class MockChannelProviderFactory : public ChannelProviderFactory
{
public:
    POINTER_DEFINITIONS(MockChannelProviderFactory);

    virtual epics::pvData::String getFactoryName()
    {
        return MockServerChannelProvider::PROVIDER_NAME;
    }

    virtual ChannelProvider::shared_pointer sharedInstance()
    {
        // no shared instance support for mock...
        return newInstance();
    }

    virtual ChannelProvider::shared_pointer newInstance()
    {
        MockServerChannelProvider::shared_pointer channelProvider(new MockServerChannelProvider());
        channelProvider->initialize();
        return channelProvider;
    }

};


static ServerContextImpl::shared_pointer ctx;

void testServer(int timeToRun)
{

    MockChannelProviderFactory::shared_pointer factory(new MockChannelProviderFactory());
    registerChannelProviderFactory(factory);

    //ServerContextImpl::shared_pointer ctx = ServerContextImpl::create();
    ctx = ServerContextImpl::create();
    ChannelAccess::shared_pointer channelAccess = getChannelAccess();
    ctx->initialize(channelAccess);

    ctx->printInfo();

    ctx->run(timeToRun);

    ctx->destroy();

    unregisterChannelProviderFactory(factory);

    structureChangedListeners.clear();
    structureStore.clear();

    ctx.reset();
}

void testServerShutdown()
{
    // NOTE: this is not thread-safe TODO
    ctx->shutdown();
}

#include <epicsGetopt.h>

void usage (char *argv[])
{
    fprintf (stderr, "\nUsage: %s [options]\n\n"
             "  -h: Help: Print this message\n"
             "\noptions:\n"
             "  -t <seconds>:   Time to run in seconds, 0 for forever\n"
             "  -d:             Enable debug output\n"
             "  -c:             Wait for clean shutdown and report used instance count (for expert users)"
             "\n\n",
             argv[0]);
}



int main(int argc, char *argv[])
{
    int opt;                    /* getopt() current option */
    bool debug = false;
    bool cleanupAndReport = false;
    int timeToRun = 0;

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */

    while ((opt = getopt(argc, argv, ":ht:dc")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage(argv);
            return 0;
        case 't':               /* Print usage */
            timeToRun = atoi(optarg);
            break;
        case 'd':               /* Debug log level */
            debug = true;
            break;
        case 'c':               /* Clean-up and report used instance count */
            cleanupAndReport = true;
            break;
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('%s -h' for help.)\n",
                    optopt, argv[0]);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('%s -h' for help.)\n",
                    optopt, argv[0]);
            return 1;
        default :
            usage(argv);
            return 1;
        }
    }

    SET_LOG_LEVEL(debug ? logLevelDebug : logLevelError);

    srand ( time(NULL) );

    testServer(timeToRun);

    shutdownSimADCs();

    cout << "Done" << endl;

    if (cleanupAndReport)
    {
        // TODO implement wait on context
        epicsThreadSleep ( 3.0 );
        //std::cout << "-----------------------------------------------------------------------" << std::endl;
        //epicsExitCallAtExits();
    }

    return (0);
}
