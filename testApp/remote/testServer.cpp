/*
 * testServer.cpp
 */

#ifdef _WIN32
#define NOMINMAX
#endif

#include <pv/serverContext.h>
#include <pv/clientContextImpl.h>
#include <epicsExit.h>
#include <pv/standardPVField.h>
#include <pv/pvTimeStamp.h>

#include <stdlib.h>
#include <time.h>
#include <vector>
#include <map>
#include <cmath>

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

Mutex structureStoreMutex;
map<String, PVStructure::shared_pointer> structureStore;

class StructureChangedCallback {
public:
    POINTER_DEFINITIONS(StructureChangedCallback);
    // TODO for now no BitSets, etc.
    virtual void structureChanged() = 0;
};

typedef map<String, vector<StructureChangedCallback::shared_pointer> > StructureChangedListenersMap;
StructureChangedListenersMap structureChangedListeners;

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
            }
            
            epicsThreadSleep(period);
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

                    // TODO lock structureStoreMutex
                    epicsGuard<epicsMutex> guard(adcSim->mutex);

                    epicsUInt32 len = adcSim->prev_nSamples;
                    double *val = adcSim->data.value.get();
                    PVDoubleArrayPtr adcSimArray = adcMatrix->getSubField<PVDoubleArray>("value");
                    PVDoubleArray::svector temp(adcSimArray->reuse());
                    temp.resize(len);
                    std::copy(val, val + len, temp.begin());
                    adcSimArray->replace(freeze(temp));

                    baseValue::shape_t* shape = &adcSim->data.shape;
                    size_t shapeLen = shape->size();

                    PVIntArrayPtr adcSimShape = adcMatrix->getSubField<PVIntArray>("dim");
                    PVIntArray::svector temp2(adcSimShape->reuse());
                    temp2.resize(shapeLen);
                    std::copy(shape->begin(), shape->end(), temp2.begin());
                    adcSimShape->replace(freeze(temp2));

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
                    // TODO lock structureStoreMutex
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



static epics::pvData::PVStructure::shared_pointer createNTTable(int columnsCount, bool timeStamp = false)
{
    StringArray fieldNames(columnsCount);
    FieldConstPtrArray fields(columnsCount);
    char sbuf[16];
    PVStringArray::svector labels(columnsCount);
    for (int i = 0; i < columnsCount; i++)
    {
        sprintf(sbuf, "column%d", i);
        fieldNames[i] = sbuf;
        fields[i] = getFieldCreate()->createScalarArray(pvDouble);
        labels[i] = sbuf;
    }

    Structure::const_shared_pointer valueStructure(
                getFieldCreate()->createStructure(fieldNames, fields)
                );

    size_t nfields = timeStamp ? 3 : 2;
    StringArray tableFieldNames(nfields);
    FieldConstPtrArray tableFields(nfields);
    tableFieldNames[0] = "labels";
    tableFields[0] = getFieldCreate()->createScalarArray(pvString);
    tableFieldNames[1] = "value";
    tableFields[1] = valueStructure;
    if (timeStamp)
    {
        tableFieldNames[2] = "timeStamp";
        tableFields[2] = getStandardField()->timeStamp();
    }

    PVStructure::shared_pointer result(
                getPVDataCreate()->createPVStructure(
                    getFieldCreate()->createStructure(
                        "uri:ev4:nt/2012/pwd:NTTable", tableFieldNames, tableFields)
                    )
                );
    result->getSubField<PVStringArray>("labels")->replace(freeze(labels));

    return result;
}

static epics::pvData::PVStructure::shared_pointer createNTNameValue(int columnsCount, bool timeStamp = false)
{
    StringArray fieldNames(columnsCount);
    FieldConstPtrArray fields(columnsCount);
    char sbuf[16];
    PVStringArray::svector labels(columnsCount);
    for (int i = 0; i < columnsCount; i++)
    {
        sprintf(sbuf, "name%d", i);
        fieldNames[i] = sbuf;
        fields[i] = getFieldCreate()->createScalarArray(pvDouble);
        labels[i] = sbuf;
    }

    size_t nfields = timeStamp ? 3 : 2;
    StringArray tableFieldNames(nfields);
    FieldConstPtrArray tableFields(nfields);
    tableFieldNames[0] = "name";
    tableFields[0] = getFieldCreate()->createScalarArray(pvString);
    tableFieldNames[1] = "value";
    tableFields[1] = getFieldCreate()->createScalarArray(pvDouble);
    if (timeStamp)
    {
        tableFieldNames[2] = "timeStamp";
        tableFields[2] = getStandardField()->timeStamp();
    }

    PVStructure::shared_pointer result(
                getPVDataCreate()->createPVStructure(
                    getFieldCreate()->createStructure(
                        "uri:ev4:nt/2012/pwd:NTNameValue", tableFieldNames, tableFields)
                    )
                );
    result->getSubField<PVStringArray>("name")->replace(freeze(labels));

    return result;
}

static epics::pvData::PVStructure::shared_pointer createNTAggregate()
{
    epics::pvData::StructureConstPtr s =
        getFieldCreate()->createFieldBuilder()->
            setId("uri:ev4:nt/2012/pwd:NTAggregate")->
            add("value", pvDouble)->
            add("N", pvLong)->
            add("dispersion", pvDouble)->
            add("first", pvDouble)->
            add("firstTimeStamp", getStandardField()->timeStamp())->
            add("last", pvDouble)->
            add("lastTimeStamp", getStandardField()->timeStamp())->
            add("max", pvDouble)->
            add("min", pvDouble)->
            createStructure();

    return getPVDataCreate()->createPVStructure(s);
}

static epics::pvData::PVStructure::shared_pointer createNTHistogram()
{
    epics::pvData::StructureConstPtr s =
        getFieldCreate()->createFieldBuilder()->
            setId("uri:ev4:nt/2012/pwd:NTHistogram")->
            addArray("ranges", pvDouble)->
            addArray("value", pvInt)->
            add("timeStamp", getStandardField()->timeStamp())->
            createStructure();

    return getPVDataCreate()->createPVStructure(s);
}

static void generateNTTableDoubleValues(epics::pvData::PVStructure::shared_pointer result)
{
    PVStringArray::shared_pointer pvLabels = (static_pointer_cast<PVStringArray>(result->getScalarArrayField("labels", pvString)));
    PVStringArray::const_svector ld(pvLabels->view());

    PVStructure::shared_pointer resultValue = result->getStructureField("value");

#define ROWS 10
    double values[ROWS];
#define FILL_VALUES(OFFSET) \
    for (int r = 0; r < ROWS; r++) \
    values[r] = rand()/((double)RAND_MAX+1) + OFFSET;

    int offset = 0;
    for (PVStringArray::const_svector::const_iterator iter = ld.begin();
         iter != ld.end();
         iter++, offset++)
    {
        FILL_VALUES(offset);
        PVDoubleArray::shared_pointer arr = resultValue->getSubField<PVDoubleArray>(*iter);
        PVDoubleArray::svector temp(arr->reuse());
        temp.resize(ROWS);
        std::copy(values, values + ROWS, temp.begin());
        arr->replace(freeze(temp));
    }
}

static void generateNTNameValueDoubleValues(epics::pvData::PVStructure::shared_pointer result)
{
    size_t len = result->getSubField<PVArray>("name")->getLength();

    PVDoubleArray::shared_pointer arr = result->getSubField<PVDoubleArray>("value");
    PVDoubleArray::svector temp(arr->reuse());
    temp.resize(len);
    for (size_t i = 0; i < len; i++)
        temp[i] = rand()/((double)RAND_MAX+1) + i;
    arr->replace(freeze(temp));
}

static void setTimeStamp(PVStructure::shared_pointer const & ts)
{
    PVTimeStamp timeStamp;
    timeStamp.attach(ts);
    TimeStamp current;
    current.getCurrent();
    timeStamp.set(current);
}

static void generateNTAggregateValues(epics::pvData::PVStructure::shared_pointer result)
{
    setTimeStamp(result->getStructureField("firstTimeStamp"));

#define N 1024
    double values[N];
    for (int r = 0; r < N; r++)
       values[r] = rand()/((double)RAND_MAX+1);
    double sum = 0;
    double min = 1;
    double max = -1;
    for (int r = 0; r < N; r++)
    {
        sum += values[r];
        if (values[r] < min) min = values[r];
        if (values[r] > max) max = values[r];
    }
    double avg = sum/N;

    sum = 0.0;
    for (int r = 0; r < N; r++)
    {
        double t = (values[r] - avg);
        sum += t*t;
    }
    double stddev = sqrt(sum/N);
    

    result->getSubField<PVDouble>("value")->put(avg);
    result->getSubField<PVDouble>("min")->put(min);
    result->getSubField<PVDouble>("max")->put(max);
    result->getSubField<PVDouble>("first")->put(values[0]);
    result->getSubField<PVDouble>("last")->put(values[N-1]);
    result->getSubField<PVDouble>("dispersion")->put(stddev);
    result->getSubField<PVLong>("N")->put(N);
#undef N

    setTimeStamp(result->getStructureField("lastTimeStamp"));
}


static void generateNTHistogramValues(epics::pvData::PVStructure::shared_pointer result)
{

#define N 100
    {
        PVDoubleArray::shared_pointer arr = result->getSubField<PVDoubleArray>("ranges");
        PVDoubleArray::svector temp(arr->reuse());
        temp.resize(N+1);
        for (size_t i = 0; i < (N+1); i++)
            temp[i] = i*10;
        arr->replace(freeze(temp));
    }
    
    {
        PVIntArray::shared_pointer arr = result->getSubField<PVIntArray>("value");
        PVIntArray::svector temp(arr->reuse());
        temp.resize(N);
        for (size_t i = 0; i < N; i++)
            temp[i] = (int32)((rand()/((double)RAND_MAX+1))*1000);
        arr->replace(freeze(temp));
    }
#undef N

    setTimeStamp(result->getStructureField("timeStamp"));
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

        PVFieldPtr ts = pvStructure->getSubField("timeStamp");
        if (ts) m_timeStamp.attach(ts);

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
        {
            ScopedLock lock(shared_from_this());
                
            if (m_pvStructure->getStructure()->getID() == "uri:ev4:nt/2012/pwd:NTTable")
            {
                generateNTTableDoubleValues(m_pvStructure);
            }
            else if (m_pvStructure->getStructure()->getID() == "uri:ev4:nt/2012/pwd:NTNameValue")
            {
                generateNTNameValueDoubleValues(m_pvStructure);
            }
            else if (m_pvStructure->getStructure()->getID() == "uri:ev4:nt/2012/pwd:NTAggregate")
            {
                generateNTAggregateValues(m_pvStructure);
            }
            else if (m_pvStructure->getStructure()->getID() == "uri:ev4:nt/2012/pwd:NTHistogram")
            {
                generateNTHistogramValues(m_pvStructure);
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

    virtual void cancel()
    {
    }

    virtual void destroy()
    {
    }
    
    virtual void lock()
    {
        structureStoreMutex.lock();
    }

    virtual void unlock()
    {
        structureStoreMutex.unlock();
    }
};


static ChannelProcess::shared_pointer getChannelProcess(
    Channel::shared_pointer const & channel,
    PVStructure::shared_pointer const & pvRequest)
{
    PVScalar::shared_pointer pvScalar = pvRequest->getSubField<PVScalar>("record._options.process");
    if (pvScalar && pvScalar->getAs<epics::pvData::boolean>())
    {
        std::tr1::shared_ptr<ChannelProcessRequesterImpl> cpr(new ChannelProcessRequesterImpl());
        return channel->createChannelProcess(cpr, PVStructure::shared_pointer());
    }
    else
        return ChannelProcess::shared_pointer(); 
}






PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelGet);

class MockChannelGet :
    public ChannelGet,
    public StructureChangedCallback,
    public std::tr1::enable_shared_from_this<MockChannelGet>
{
private:
    String m_channelName;
    ChannelGetRequester::shared_pointer m_channelGetRequester;
    PVStructure::shared_pointer m_pvStructure;
    BitSet::shared_pointer m_bitSet;
    ChannelProcess::shared_pointer m_channelProcess;
    AtomicBoolean m_changed;

protected:
    MockChannelGet(Channel::shared_pointer const & channel,
                   ChannelGetRequester::shared_pointer const & channelGetRequester,
                   PVStructure::shared_pointer const & pvStructure,
                   PVStructure::shared_pointer const & pvRequest) :
        m_channelName(channel->getChannelName()),
        m_channelGetRequester(channelGetRequester),
        m_pvStructure(getRequestedStructure(pvStructure, pvRequest)),
        m_bitSet(new BitSet(m_pvStructure->getNumberFields())),
        m_channelProcess(getChannelProcess(channel, pvRequest))
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelGet);
        m_changed.set();    // initial value
    }

public:
    static ChannelGet::shared_pointer create(
                Channel::shared_pointer const & channel,
                ChannelGetRequester::shared_pointer const & channelGetRequester,
                PVStructure::shared_pointer const & pvStructure,
                PVStructure::shared_pointer const & pvRequest)
    {
        ChannelGet::shared_pointer thisPtr(new MockChannelGet(channel, channelGetRequester, pvStructure, pvRequest));

        // register
        structureChangedListeners[channel->getChannelName()].push_back(std::tr1::dynamic_pointer_cast<StructureChangedCallback>(thisPtr));

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
        if (m_channelProcess)
            m_channelProcess->process(false);

        // TODO far from being thread-safe
        if (m_changed.get())
        {
            m_bitSet->set(0);
            m_changed.clear();
        }
        else
            m_bitSet->clear(0);
            
        m_channelGetRequester->getDone(Status::Ok);

        if (lastRequest)
            destroy();
    }

    virtual void structureChanged()
    {
        m_changed.set();
    }

    virtual void cancel()
    {
    }

    virtual void destroy()
    {
        if (m_channelProcess)
            m_channelProcess->destroy();

        // remove itself from listeners table
        if (structureChangedListeners.count(m_channelName))
        {
            vector<StructureChangedCallback::shared_pointer> &vec = structureChangedListeners[m_channelName];
            for (vector<StructureChangedCallback::shared_pointer>::iterator i = vec.begin();
                 i != vec.end(); i++)
            {
                if (i->get() == this)
                {
                    vec.erase(i);
                    break;
                }
            }
        }        
    }

    virtual void lock()
    {
        structureStoreMutex.lock();
    }

    virtual void unlock()
    {
        structureStoreMutex.unlock();
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
    ChannelProcess::shared_pointer m_channelProcess;

protected:
    MockChannelPut(Channel::shared_pointer const & channel,
                   ChannelPutRequester::shared_pointer const & channelPutRequester,
                   PVStructure::shared_pointer const & pvStructure,
                   PVStructure::shared_pointer const & pvRequest) :
        m_channelName(channel->getChannelName()),
        m_channelPutRequester(channelPutRequester),
        m_pvStructure(getRequestedStructure(pvStructure, pvRequest)),
        m_bitSet(new BitSet(m_pvStructure->getNumberFields())),
        m_channelProcess(getChannelProcess(channel, pvRequest))
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelPut);
    }

public:
    static ChannelPut::shared_pointer create(
            Channel::shared_pointer const & channel,
            ChannelPutRequester::shared_pointer const & channelPutRequester,
            PVStructure::shared_pointer const & pvStructure,
            PVStructure::shared_pointer const & pvRequest)
    {
        ChannelPut::shared_pointer thisPtr(new MockChannelPut(channel, channelPutRequester, pvStructure, pvRequest));
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
        if (m_channelProcess)
            m_channelProcess->process(false);

        m_channelPutRequester->putDone(Status::Ok);

        notifyStructureChanged(m_channelName);

        if (lastRequest)
            destroy();
    }

    virtual void get()
    {
        m_channelPutRequester->getDone(Status::Ok);
    }

    virtual void cancel()
    {
    }

    virtual void destroy()
    {
        if (m_channelProcess)
            m_channelProcess->destroy();
    }

    virtual void lock()
    {
        structureStoreMutex.lock();
    }

    virtual void unlock()
    {
        structureStoreMutex.unlock();
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
    ChannelProcess::shared_pointer m_channelProcess;

protected:
    MockChannelPutGet(Channel::shared_pointer const & channel,
                      ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
                      PVStructure::shared_pointer const & pvStructure,
                      PVStructure::shared_pointer const & pvRequest) :
        m_channelName(channel->getChannelName()),
        m_channelPutGetRequester(channelPutGetRequester),
        m_getStructure(getRequestedStructure(pvStructure, pvRequest, "getField")),
        m_putStructure(getRequestedStructure(pvStructure, pvRequest, "putField")),
        m_channelProcess(getChannelProcess(channel, pvRequest))

    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelPutGet);
    }

public:
    static ChannelPutGet::shared_pointer create(
            Channel::shared_pointer const & channel,
            ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
            PVStructure::shared_pointer const & pvStructure,
            PVStructure::shared_pointer const & pvRequest)
    {
        ChannelPutGet::shared_pointer thisPtr(new MockChannelPutGet(channel, channelPutGetRequester, pvStructure, pvRequest));

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
        if (m_channelProcess)
            m_channelProcess->process(false);

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

    virtual void cancel()
    {
    }

    virtual void destroy()
    {
        if (m_channelProcess)
            m_channelProcess->destroy();
    }

    virtual void lock()
    {
        structureStoreMutex.lock();
    }

    virtual void unlock()
    {
        structureStoreMutex.unlock();
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
                PVStringArray::svector labels(columnsCount);
                for (int i = 0; i < columnsCount; i++)
                {
                    sprintf(sbuf, "name%d", i);
                    labels[i] = sbuf;
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
                result->getSubField<PVStringArray>("name")->replace(freeze(labels));

                int32 len = columnsCount;
                PVDoubleArray::svector mv(len);
                for (int r = 0; r < len; r++)
                    mv[r] = rand()/((double)RAND_MAX+1) + (int)(r);
                result->getSubField<PVDoubleArray>("value")->replace(freeze(mv));

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
                PVIntArray::svector dimValues(2);
                dimValues[0] = rowsVal;
                dimValues[1] = colsVal;
                result->getSubField<PVIntArray>("dim")->replace(freeze(dimValues));

                PVStringPtr byColumns = static_pointer_cast<PVString>(args->getSubField("bycolumns"));
                bool bycolumns = (byColumns.get() && byColumns->get() == "1");

                int32 len = rowsVal * colsVal;
                PVDoubleArray::svector mv(len);
                for (int r = 0; r < len; r++)
                    if (bycolumns)
                        mv[r] = rand()/((double)RAND_MAX+1) + (int)(r%rowsVal);
                    else
                        mv[r] = rand()/((double)RAND_MAX+1) + (int)(r/colsVal);
                result->getSubField<PVDoubleArray>("value")->replace(freeze(mv));

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
                        PVByteArray::svector temp(value->reuse());
                        temp.resize(fileSize);
                        in.readsome((char*)temp.data(), fileSize);
                        value->replace(freeze(temp));

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
        else if (m_channelName == "testSum") {

            PVStructure::shared_pointer args(
                        (pvArgument->getStructure()->getID() == "uri:ev4:nt/2012/pwd:NTURI") ?
                            pvArgument->getStructureField("query") :
                            pvArgument
                            );

            const String helpText =
                    "Calculates a sum of two integer values.\n"
                    "Arguments:\n"
                    "\tint a\tfirst integer number\n"
                    "\tint b\tsecond integer number\n";
            if (handleHelp(args, m_channelRPCRequester, helpText))
                return;

            PVInt::shared_pointer pa = args->getSubField<PVInt>("a");
            PVInt::shared_pointer pb = args->getSubField<PVInt>("b");
            if (!pa || !pb)
            {
                PVStructure::shared_pointer nullPtr;
                Status errorStatus(Status::STATUSTYPE_ERROR, "int a and int b arguments are required");
                m_channelRPCRequester->requestDone(errorStatus, nullPtr);
                return;
            }

            int a = pa->get();
            int b = pb->get();

            FieldCreatePtr fieldCreate = getFieldCreate();

            StringArray fieldNames;
            fieldNames.push_back("c");
            FieldConstPtrArray fields;
            fields.push_back(fieldCreate->createScalar(pvInt));
            StructureConstPtr resultStructure = fieldCreate->createStructure(fieldNames, fields);

            PVStructure::shared_pointer result = getPVDataCreate()->createPVStructure(resultStructure);
            result->getIntField("c")->put(a+b);

            m_channelRPCRequester->requestDone(Status::Ok, result);

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

    virtual void cancel()
    {
    }

    virtual void destroy()
    {
    }

    virtual void lock()
    {
        structureStoreMutex.lock();
    }

    virtual void unlock()
    {
        structureStoreMutex.unlock();
    }
};










PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelArray);

class MockChannelArray : public ChannelArray
{
private:
    ChannelArrayRequester::shared_pointer m_channelArrayRequester;
    PVArray::shared_pointer m_pvArray;
    PVArray::shared_pointer m_pvStructureArray;

protected:
    MockChannelArray(ChannelArrayRequester::shared_pointer const & channelArrayRequester,
                     PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & /*pvRequest*/) :
        m_channelArrayRequester(channelArrayRequester)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelArray);

        m_pvStructureArray = pvStructure->getSubField<PVArray>("value");
        if (m_pvStructureArray != 0)
            m_pvArray = std::tr1::dynamic_pointer_cast<PVArray>(
                getPVDataCreate()->createPVField(m_pvStructureArray->getField()));
    }

public:
    static ChannelArray::shared_pointer create(ChannelArrayRequester::shared_pointer const & channelArrayRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        ChannelArray::shared_pointer thisPtr(new MockChannelArray(channelArrayRequester, pvStructure, pvRequest));

        PVArray::shared_pointer array(static_cast<MockChannelArray*>(thisPtr.get())->m_pvArray);
        if (array.get())
            channelArrayRequester->channelArrayConnect(Status::Ok, thisPtr, array);
        else
        {
            Status errorStatus(Status::STATUSTYPE_ERROR, "no 'value' subfield of array type");
            channelArrayRequester->channelArrayConnect(errorStatus, thisPtr, array);
        }
        
        return thisPtr;
    }

    virtual ~MockChannelArray()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannelArray);
    }

    template<typename APVF>
    void put(PVArray::shared_pointer const & pvfrom,
             PVArray::shared_pointer const & pvto,
             size_t offset, size_t count)
    {
        typename APVF::shared_pointer from = std::tr1::static_pointer_cast<APVF>(pvfrom);
        typename APVF::shared_pointer to = std::tr1::static_pointer_cast<APVF>(pvto);
       
        typename APVF::const_svector ref(from->view());
        if (offset > ref.size())
	       offset = ref.size();
        if (count + offset > ref.size())
	       count = ref.size() - offset;
	 
        typename APVF::svector temp(to->reuse());
        if (offset + count > temp.size())
            temp.resize(offset + count);
 
        std::copy(ref.begin(), ref.begin() + count, temp.begin() + offset);

        to->replace(freeze(temp));
    }

    virtual void putArray(bool lastRequest, size_t offset, size_t count)
    {
        size_t o = offset;
        if (count == 0) count = m_pvArray->getLength();
        size_t c = count;

        Field::const_shared_pointer field = m_pvArray->getField();
        Type type = field->getType();
        if (type == scalarArray)
        {
            switch (std::tr1::static_pointer_cast<const ScalarArray>(field)->getElementType())
            {
                case pvBoolean: put<PVBooleanArray>(m_pvArray, m_pvStructureArray, o, c); break;
                case pvByte: put<PVByteArray>(m_pvArray, m_pvStructureArray, o, c); break;
                case pvShort: put<PVShortArray>(m_pvArray, m_pvStructureArray, o, c); break;
                case pvInt: put<PVIntArray>(m_pvArray, m_pvStructureArray, o, c); break;
                case pvLong: put<PVLongArray>(m_pvArray, m_pvStructureArray, o, c); break;
                case pvUByte: put<PVUByteArray>(m_pvArray, m_pvStructureArray, o, c); break;
                case pvUShort: put<PVUShortArray>(m_pvArray, m_pvStructureArray, o, c); break;
                case pvUInt: put<PVUIntArray>(m_pvArray, m_pvStructureArray, o, c); break;
                case pvULong: put<PVULongArray>(m_pvArray, m_pvStructureArray, o, c); break;
                case pvFloat: put<PVFloatArray>(m_pvArray, m_pvStructureArray, o, c); break;
                case pvDouble: put<PVDoubleArray>(m_pvArray, m_pvStructureArray, o, c); break;
                case pvString: put<PVStringArray>(m_pvArray, m_pvStructureArray, o, c); break;
            }
        }
        else if (type == structureArray)
            put<PVStructureArray>(m_pvArray, m_pvStructureArray, o, c);
        else if (type == unionArray)
            put<PVUnionArray>(m_pvArray, m_pvStructureArray, o, c);

        m_channelArrayRequester->putArrayDone(Status::Ok);
        if (lastRequest)
            destroy();
    }

    template<typename APVF>
    void get(PVArray::shared_pointer const & pvfrom,
             PVArray::shared_pointer const & pvto,
             size_t offset, size_t count)
    {
        typename APVF::shared_pointer from = std::tr1::static_pointer_cast<APVF>(pvfrom);
        typename APVF::shared_pointer to = std::tr1::static_pointer_cast<APVF>(pvto);
        
        // TODO range check

        typename APVF::const_svector temp(from->view());
        temp.slice(offset, count);
        to->replace(temp);
    }
    

    virtual void getArray(bool lastRequest, size_t offset, size_t count)
    {
        size_t o = offset;
        if (count == 0) count = m_pvStructureArray->getLength();
        size_t c = count;

        Field::const_shared_pointer field = m_pvArray->getField();
        Type type = field->getType();
        if (type == scalarArray)
        {
            switch (std::tr1::static_pointer_cast<const ScalarArray>(field)->getElementType())
            {
                case pvBoolean: get<PVBooleanArray>(m_pvStructureArray, m_pvArray, o, c); break;
                case pvByte: get<PVByteArray>(m_pvStructureArray, m_pvArray, o, c); break;
                case pvShort: get<PVShortArray>(m_pvStructureArray, m_pvArray, o, c); break;
                case pvInt: get<PVIntArray>(m_pvStructureArray, m_pvArray, o, c); break;
                case pvLong: get<PVLongArray>(m_pvStructureArray, m_pvArray, o, c); break;
                case pvUByte: get<PVUByteArray>(m_pvStructureArray, m_pvArray, o, c); break;
                case pvUShort: get<PVUShortArray>(m_pvStructureArray, m_pvArray, o, c); break;
                case pvUInt: get<PVUIntArray>(m_pvStructureArray, m_pvArray, o, c); break;
                case pvULong: get<PVULongArray>(m_pvStructureArray, m_pvArray, o, c); break;
                case pvFloat: get<PVFloatArray>(m_pvStructureArray, m_pvArray, o, c); break;
                case pvDouble: get<PVDoubleArray>(m_pvStructureArray, m_pvArray, o, c); break;
                case pvString: get<PVStringArray>(m_pvStructureArray, m_pvArray, o, c); break;
            }
        }
        else if (type == structureArray)
            get<PVStructureArray>(m_pvStructureArray, m_pvArray, o, c);
        else if (type == unionArray)
            get<PVUnionArray>(m_pvStructureArray, m_pvArray, o, c);
        
        m_channelArrayRequester->getArrayDone(Status::Ok);
        if (lastRequest)
            destroy();
    }

    virtual void setLength(bool lastRequest, size_t length, size_t capacity)
    {
        if (capacity > 0) {
            m_pvStructureArray->setCapacity(capacity);
        }
        
        m_pvStructureArray->setLength(length);
        
        m_channelArrayRequester->setLengthDone(Status::Ok);
        if (lastRequest)
            destroy();
    }

    virtual void cancel()
    {
    }

    virtual void destroy()
    {
    }

    virtual void lock()
    {
        structureStoreMutex.lock();
    }

    virtual void unlock()
    {
        structureStoreMutex.unlock();
    }
};






PVACCESS_REFCOUNT_MONITOR_DEFINE(mockMonitor);

class MockMonitor :
    public Monitor,
    public StructureChangedCallback,
    public std::tr1::enable_shared_from_this<MockMonitor>
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
    AtomicBoolean m_active;


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

        // register
        structureChangedListeners[channelName].push_back(std::tr1::dynamic_pointer_cast<StructureChangedCallback>(thisPtr));

        StructureConstPtr structurePtr = static_cast<MockMonitor*>(thisPtr.get())->m_pvStructure->getStructure();
        monitorRequester->monitorConnect(Status::Ok, thisPtr, structurePtr);

        return thisPtr;
    }

    virtual ~MockMonitor()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockMonitor);
    }

    virtual Status start()
    {
        m_active.set();
        
        // first monitor
        Monitor::shared_pointer thisPtr = shared_from_this();
        m_monitorRequester->monitorEvent(thisPtr);

        return Status::Ok;
    }

    virtual Status stop()
    {
        m_active.clear();
        return Status::Ok;
    }

    virtual void structureChanged()
    {
        if (m_active.get())
        {   
            {
	        Lock xx(m_lock);
                m_count = 0;
            }
            Monitor::shared_pointer thisPtr = shared_from_this();
            m_monitorRequester->monitorEvent(thisPtr);
        }
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

    virtual void cancel()
    {
    }

    virtual void destroy()
    {
        stop();

        // remove itself from listeners table
        if (structureChangedListeners.count(m_channelName))
        {
            vector<StructureChangedCallback::shared_pointer> &vec = structureChangedListeners[m_channelName];
            for (vector<StructureChangedCallback::shared_pointer>::iterator i = vec.begin();
                 i != vec.end(); i++)
            {
                if (i->get() == this)
                {
                    vec.erase(i);
                    break;
                }
            }
        }        
    }

    virtual void lock()
    {
        structureStoreMutex.lock();
    }

    virtual void unlock()
    {
        structureStoreMutex.unlock();
    }

};


PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannel);

class MockChannel :
    public Channel,
    public std::tr1::enable_shared_from_this<MockChannel>
{
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
                    PVDoubleArray::svector temp(pvField->reuse());
                    temp.resize(specCount);

                    double v = 0;
                    for (int n = 0; n < specCount; n++)
                    {
                        temp[n] = v; v+=1;
                    }
                    pvField->replace(freeze(temp));
                }
                else
                {
                    PVDoubleArray::svector temp(pvField->reuse());
                    specCount = 1024*1024;
                    temp.resize(specCount);

                    double v = 0;
                    for (int n = 0; n < specCount; n++)
                    {
                        temp[n] = v; v+=1.1;
                    }
                    pvField->replace(freeze(temp));
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
                m_pvStructure = createNTTable(5, true);   // 5 columns w/ timeStamp
                generateNTTableDoubleValues(m_pvStructure);
            }
            else if (m_name.find("testNameValue") == 0)
            {
                m_pvStructure = createNTNameValue(5, true);   // 5 columns w/ timeStamp
                generateNTNameValueDoubleValues(m_pvStructure);
            }
            else if (m_name.find("testAggregate") == 0)
            {
                m_pvStructure = createNTAggregate();
                generateNTAggregateValues(m_pvStructure);
            }
            else if (m_name.find("testHistogram") == 0)
            {
                m_pvStructure = createNTHistogram();
                generateNTHistogramValues(m_pvStructure);
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
                PVIntArray::svector dimValue(1);
                dimValue[0] = 0;
                m_pvStructure->getSubField<PVIntArray>("dim")->replace(freeze(dimValue));

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
            else if (m_name == "testCounter" || m_name == "testSimpleCounter")
            {
                String allProperties("timeStamp");
                m_pvStructure = getStandardPVField()->scalar(pvInt,allProperties);
            }
            else if (m_name == "testEnum")
            {
                StringArray choices;
                choices.push_back("zeroValue");
                choices.push_back("oneValue");
                choices.push_back("twoValue");
                choices.push_back("threeValue");
                choices.push_back("fourValue");
                choices.push_back("fiveValue");
                choices.push_back("sixValue");
                choices.push_back("sevenValue");
                String allProperties("timeStamp");
                m_pvStructure = getStandardPVField()->enumerated(choices,allProperties);
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
        return MockChannelGet::create(shared_from_this(), channelGetRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelPut::shared_pointer createChannelPut(
            ChannelPutRequester::shared_pointer const & channelPutRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
        return MockChannelPut::create(shared_from_this(), channelPutRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelPutGet::shared_pointer createChannelPutGet(
            ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
        return MockChannelPutGet::create(shared_from_this(), channelPutGetRequester, m_pvStructure, pvRequest);
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

        c = MockChannel::create(chProviderPtr, cr, "testNameValue", "local");
        process = c->createChannelProcess(cpr, PVStructure::shared_pointer());
        m_scan1Hz.toProcess.push_back(process);

        c = MockChannel::create(chProviderPtr, cr, "testAggregate", "local");
        process = c->createChannelProcess(cpr, PVStructure::shared_pointer());
        m_scan1Hz.toProcess.push_back(process);

        c = MockChannel::create(chProviderPtr, cr, "testHistogram", "local");
        process = c->createChannelProcess(cpr, PVStructure::shared_pointer());
        m_scan1Hz.toProcess.push_back(process);

        m_scan1HzThread.reset(new epics::pvData::Thread("process1hz", highPriority, &m_scan1Hz));
        m_scan10HzThread.reset(new epics::pvData::Thread("process10hz", highPriority, &m_scan10Hz));

        m_adcChannel = MockChannel::create(chProviderPtr, cr, "testADC", "local");
        m_adcAction.name = "testADC";
        m_adcAction.adcMatrix = static_pointer_cast<MockChannel>(m_adcChannel)->m_pvStructure;
        m_adcAction.adcSim = createSimADC("testADC");
        m_adcThread.reset(new epics::pvData::Thread("adcThread", highPriority, &m_adcAction));

        m_mpChannel = MockChannel::create(chProviderPtr, cr, "testMP", "local");
        m_imgAction.name = "testMP";
        m_imgAction.pvImage = static_pointer_cast<MockChannel>(m_mpChannel)->m_pvStructure;
        m_imgThread.reset(new epics::pvData::Thread("imgThread", highPriority, &m_imgAction));
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
    auto_ptr<epics::pvData::Thread> m_scan1HzThread;

    ProcessAction m_scan10Hz;
    auto_ptr<epics::pvData::Thread> m_scan10HzThread;

    ADCAction m_adcAction;
    auto_ptr<epics::pvData::Thread> m_adcThread;

    NTImageAction m_imgAction;
    auto_ptr<epics::pvData::Thread> m_imgThread;
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


#ifndef TESTSERVERNOMAIN 

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
#endif
