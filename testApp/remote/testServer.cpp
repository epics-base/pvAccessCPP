/*
 * testServer.cpp
 */

// disable buggy boost enable_shared_from_this assert code
#define BOOST_DISABLE_ASSERTS

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

// s1 starts with s2 check
bool starts_with(const string& s1, const string& s2) {
    return s2.size() <= s1.size() && s1.compare(0, s2.size(), s2) == 0;
}


Mutex structureStoreMutex;
map<string, PVStructure::shared_pointer> structureStore;

class StructureChangedCallback {
public:
    POINTER_DEFINITIONS(StructureChangedCallback);

    virtual ~StructureChangedCallback() {}

    // TODO for now no BitSets, etc.
    virtual void structureChanged() = 0;
};

typedef map<string, vector<StructureChangedCallback::shared_pointer> > StructureChangedListenersMap;
StructureChangedListenersMap structureChangedListeners;

static void notifyStructureChanged(std::string const & name)
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
    string subfieldName = "field")
{
    // if pvRequest is empty, just use pvStructure
    if (pvRequest.get() && pvRequest->getPVFields().size() > 0)
    {
        PVStructure::shared_pointer pvRequestFields;
        if (pvRequest->getSubField(subfieldName))
            pvRequestFields = pvRequest->getSubField<PVStructure>(subfieldName);
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
                    (*iter)->process();
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
    string name;
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

                    PVStructure::shared_pointer ts = adcMatrix->getSubField<PVStructure>("timeStamp");

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


// testNTNDArray
class NTNDArrayAction : public Runnable {
public:
    string name;
    PVStructure::shared_pointer pvImage;
    float angle;
    double period;

    AtomicBoolean stopped;

    NTNDArrayAction(double periodHz) :
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
                    std::cerr << "Unhandled exception caught in NTNDArrayAction::run(): " << ex.what() << std::endl;
                } catch (...) {
                    std::cerr << "Unhandled exception caught in NTNDArrayAction::run()" << std::endl;
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
                "epics:nt/NTTable:1.0", tableFieldNames, tableFields)
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
                "epics:nt/NTNameValue:1.0", tableFieldNames, tableFields)
        )
    );
    result->getSubField<PVStringArray>("name")->replace(freeze(labels));

    return result;
}

static epics::pvData::PVStructure::shared_pointer createNTAggregate()
{
    epics::pvData::StructureConstPtr s =
        getFieldCreate()->createFieldBuilder()->
        setId("epics:nt/NTAggregate:1.0")->
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
        setId("epics:nt/NTHistogram:1.0")->
        addArray("ranges", pvDouble)->
        addArray("value", pvInt)->
        add("timeStamp", getStandardField()->timeStamp())->
        createStructure();

    return getPVDataCreate()->createPVStructure(s);
}

static void generateNTTableDoubleValues(epics::pvData::PVStructure::shared_pointer result)
{
    PVStringArray::shared_pointer pvLabels = result->getSubField<PVStringArray>("labels");
    PVStringArray::const_svector ld(pvLabels->view());

    PVStructure::shared_pointer resultValue = result->getSubField<PVStructure>("value");

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
    setTimeStamp(result->getSubField<PVStructure>("firstTimeStamp"));

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

    setTimeStamp(result->getSubField<PVStructure>("lastTimeStamp"));
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

    setTimeStamp(result->getSubField<PVStructure>("timeStamp"));
}

class ChannelFindRequesterImpl : public ChannelFindRequester
{
    virtual void channelFindResult(epics::pvData::Status const & status,
                                   ChannelFind::shared_pointer const & /*channelFind*/, bool wasFound)
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

    virtual void channelCreated(epics::pvData::Status const & /*status*/, Channel::shared_pointer const & /*channel*/)
    {
        //std::cout << "channelCreated(" << status << ", "
        //          << (channel ? channel->getChannelName() : "(null)") << ")" << std::endl;
    }

    virtual void channelStateChange(Channel::shared_pointer const & /*channel*/, Channel::ConnectionState /*connectionState*/)
    {
        //std::cout << "channelStateChange(" << channel->getChannelName() << ", " << Channel::ConnectionStateNames[connectionState] << ")" << std::endl;
    }
};

class ChannelProcessRequesterImpl : public ChannelProcessRequester
{
    //TODO weak ChannelProcess::shared_pointer m_channelProcess;

    virtual string getRequesterName()
    {
        return "ProcessRequesterImpl";
    };

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelProcessConnect(const epics::pvData::Status& /*status*/,
                                       ChannelProcess::shared_pointer const & /*channelProcess*/)
    {
        //std::cout << "channelProcessConnect(" << status << ")" << std::endl;

        //m_channelProcess = channelProcess;
    }

    virtual void processDone(const epics::pvData::Status& /*status*/,
                             ChannelProcess::shared_pointer const &)
    {
        //std::cout << "processDone(" << status << ")" << std::endl;
    }

};

class MockChannelProcess :
    public ChannelProcess,
    public std::tr1::enable_shared_from_this<MockChannelProcess>
{
private:
    Channel::shared_pointer m_channel;
    ChannelProcessRequester::shared_pointer m_channelProcessRequester;
    PVStructure::shared_pointer m_pvStructure;
    PVScalarPtr m_valueField;
    PVTimeStamp m_timeStamp;
    AtomicBoolean m_lastRequest;

protected:
    MockChannelProcess(Channel::shared_pointer const & channel, ChannelProcessRequester::shared_pointer const & channelProcessRequester,
                       PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & /*pvRequest*/) :
        m_channel(channel), m_channelProcessRequester(channelProcessRequester), m_pvStructure(pvStructure)
    {
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
        Channel::shared_pointer const & channel,
        ChannelProcessRequester::shared_pointer const & channelProcessRequester,
        PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        // TODO use std::make_shared
        std::tr1::shared_ptr<MockChannelProcess> tp(
            new MockChannelProcess(channel, channelProcessRequester, pvStructure, pvRequest)
        );
        ChannelProcess::shared_pointer thisPtr = tp;
        // TODO pvRequest
        channelProcessRequester->channelProcessConnect(Status::Ok, thisPtr);

        return thisPtr;
    }

    virtual ~MockChannelProcess()
    {
    }


    virtual void process()
    {
        {
            ScopedLock lock(shared_from_this());

            if (starts_with(m_pvStructure->getStructure()->getID(), "epics:nt/NTTable:1."))
            {
                generateNTTableDoubleValues(m_pvStructure);
            }
            else if (starts_with(m_pvStructure->getStructure()->getID(), "epics:nt/NTNameValue:1."))
            {
                generateNTNameValueDoubleValues(m_pvStructure);
            }
            else if (starts_with(m_pvStructure->getStructure()->getID(), "epics:nt/NTAggregate:1."))
            {
                generateNTAggregateValues(m_pvStructure);
            }
            else if (starts_with(m_pvStructure->getStructure()->getID(), "epics:nt/NTHistogram:1."))
            {
                generateNTHistogramValues(m_pvStructure);
            }
            else if (starts_with(m_pvStructure->getStructure()->getID(), "epics:test/binaryCounter:1."))
            {
                PVBytePtr pvByte = static_pointer_cast<PVByte>(m_valueField);
                int8 val = pvByte->get() + 1;
                pvByte->put(val);

                m_pvStructure->getSubField<PVBoolean>("bit0")->put(val & (1 << 0));
                m_pvStructure->getSubField<PVBoolean>("bit1")->put(val & (1 << 1));
                m_pvStructure->getSubField<PVBoolean>("bit2")->put(val & (1 << 2));
                m_pvStructure->getSubField<PVBoolean>("bit3")->put(val & (1 << 3));
                m_pvStructure->getSubField<PVBoolean>("bit4")->put(val & (1 << 4));
                m_pvStructure->getSubField<PVBoolean>("bit5")->put(val & (1 << 5));
                m_pvStructure->getSubField<PVBoolean>("bit6")->put(val & (1 << 6));
                m_pvStructure->getSubField<PVBoolean>("bit7")->put(val & (1 << 7));
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
                    string val = pvString->get();
                    if (val.empty())
                        pvString->put("gen0");
                    else
                    {
                        char c = val[3];
                        c++;
                        pvString->put("gen" + string(1, c));
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

        m_channelProcessRequester->processDone(Status::Ok, shared_from_this());

        notifyStructureChanged(m_channel->getChannelName());

        if (m_lastRequest.get())
            destroy();
    }

    virtual void lastRequest()
    {
        m_lastRequest.set();
    }

    virtual Channel::shared_pointer getChannel()
    {
        return m_channel;
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







class MockChannelGet :
    public ChannelGet,
    public StructureChangedCallback,
    public std::tr1::enable_shared_from_this<MockChannelGet>
{
private:
    Channel::shared_pointer m_channel;
    ChannelGetRequester::shared_pointer m_channelGetRequester;
    bool m_alwaysSendAll;
    PVStructure::shared_pointer m_pvStructure;
    BitSet::shared_pointer m_bitSet;
    ChannelProcess::shared_pointer m_channelProcess;
    AtomicBoolean m_changed;
    AtomicBoolean m_lastRequest;

protected:
    MockChannelGet(Channel::shared_pointer const & channel,
                   ChannelGetRequester::shared_pointer const & channelGetRequester,
                   PVStructure::shared_pointer const & pvStructure,
                   PVStructure::shared_pointer const & pvRequest) :
        m_channel(channel),
        m_channelGetRequester(channelGetRequester),
        m_alwaysSendAll(false),
        m_pvStructure(getRequestedStructure(pvStructure, pvRequest)),
        m_bitSet(new BitSet(m_pvStructure->getNumberFields())),
        m_channelProcess(getChannelProcess(channel, pvRequest))
    {
        PVScalar::shared_pointer pvScalar = pvRequest->getSubField<PVScalar>("record._options.alwaysSendAll");
        if (pvScalar)
            m_alwaysSendAll = pvScalar->getAs<epics::pvData::boolean>();

        m_changed.set();    // initial value
    }

public:
    static ChannelGet::shared_pointer create(
        Channel::shared_pointer const & channel,
        ChannelGetRequester::shared_pointer const & channelGetRequester,
        PVStructure::shared_pointer const & pvStructure,
        PVStructure::shared_pointer const & pvRequest)
    {
        // TODO use std::make_shared
        std::tr1::shared_ptr<MockChannelGet> tp(
            new MockChannelGet(channel, channelGetRequester, pvStructure, pvRequest)
        );
        ChannelGet::shared_pointer thisPtr = tp;

        // register
        structureChangedListeners[channel->getChannelName()].push_back(std::tr1::dynamic_pointer_cast<StructureChangedCallback>(thisPtr));

        channelGetRequester->channelGetConnect(Status::Ok, thisPtr,
                                               static_cast<MockChannelGet*>(thisPtr.get())->m_pvStructure->getStructure());
        return thisPtr;
    }

    virtual ~MockChannelGet()
    {
    }

    virtual void get()
    {
        if (m_channelProcess)
            m_channelProcess->process();

        // TODO far from being thread-safe
        if (m_alwaysSendAll || m_changed.get())
        {
            m_bitSet->set(0);
            m_changed.clear();
        }
        else
            m_bitSet->clear(0);

        m_channelGetRequester->getDone(Status::Ok, shared_from_this(), m_pvStructure, m_bitSet);

        if (m_lastRequest.get())
            destroy();
    }

    virtual void structureChanged()
    {
        m_changed.set();
    }

    virtual void lastRequest()
    {
        m_lastRequest.set();
    }

    virtual Channel::shared_pointer getChannel()
    {
        return m_channel;
    }

    virtual void cancel()
    {
    }

    virtual void destroy()
    {
        if (m_channelProcess)
            m_channelProcess->destroy();

        // remove itself from listeners table
        if (structureChangedListeners.count(m_channel->getChannelName()))
        {
            vector<StructureChangedCallback::shared_pointer> &vec = structureChangedListeners[m_channel->getChannelName()];
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




class MockChannelPut :
    public ChannelPut,
    public std::tr1::enable_shared_from_this<MockChannelPut>
{
private:
    Channel::shared_pointer m_channel;
    ChannelPutRequester::shared_pointer m_channelPutRequester;
    PVStructure::shared_pointer m_pvStructure;
    BitSet::shared_pointer m_bitSet;
    ChannelProcess::shared_pointer m_channelProcess;
    AtomicBoolean m_lastRequest;

protected:
    MockChannelPut(Channel::shared_pointer const & channel,
                   ChannelPutRequester::shared_pointer const & channelPutRequester,
                   PVStructure::shared_pointer const & pvStructure,
                   PVStructure::shared_pointer const & pvRequest) :
        m_channel(channel),
        m_channelPutRequester(channelPutRequester),
        m_pvStructure(getRequestedStructure(pvStructure, pvRequest)),
        m_bitSet(new BitSet(m_pvStructure->getNumberFields())),
        m_channelProcess(getChannelProcess(channel, pvRequest))
    {
        m_bitSet->set(0);
    }

public:
    static ChannelPut::shared_pointer create(
        Channel::shared_pointer const & channel,
        ChannelPutRequester::shared_pointer const & channelPutRequester,
        PVStructure::shared_pointer const & pvStructure,
        PVStructure::shared_pointer const & pvRequest)
    {
        // TODO use std::make_shared
        std::tr1::shared_ptr<MockChannelPut> tp(
            new MockChannelPut(channel, channelPutRequester, pvStructure, pvRequest)
        );
        ChannelPut::shared_pointer thisPtr = tp;
        channelPutRequester->channelPutConnect(Status::Ok, thisPtr,
                                               static_cast<MockChannelPut*>(thisPtr.get())->m_pvStructure->getStructure());
        return thisPtr;
    }

    virtual ~MockChannelPut()
    {
    }


    virtual void put(PVStructure::shared_pointer const & pvPutStructure, BitSet::shared_pointer const & putBitSet)
    {
        // TODO use putBitSet and do not copy all
        // (note that server code has already not deserialized fields whose bits are not set)
        if (putBitSet->cardinality())
        {
            lock();
            m_pvStructure->copyUnchecked(*pvPutStructure);
            unlock();
        }

        if (m_channelProcess)
            m_channelProcess->process();

        m_channelPutRequester->putDone(Status::Ok, shared_from_this());

        notifyStructureChanged(m_channel->getChannelName());

        if (m_lastRequest.get())
            destroy();
    }

    virtual void get()
    {
        // NOTE: alwasy returns entire m_bitSet
        m_channelPutRequester->getDone(Status::Ok, shared_from_this(), m_pvStructure, m_bitSet);

        if (m_lastRequest.get())
            destroy();
    }

    virtual void cancel()
    {
    }

    virtual void lastRequest()
    {
        m_lastRequest.set();
    }

    virtual Channel::shared_pointer getChannel()
    {
        return m_channel;
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




class MockChannelPutGet :
    public ChannelPutGet,
    public std::tr1::enable_shared_from_this<ChannelPutGet>
{
private:
    Channel::shared_pointer m_channel;
    ChannelPutGetRequester::shared_pointer m_channelPutGetRequester;
    PVStructure::shared_pointer m_getStructure;
    BitSet::shared_pointer m_getBitSet;
    PVStructure::shared_pointer m_putStructure;
    BitSet::shared_pointer m_putBitSet;
    ChannelProcess::shared_pointer m_channelProcess;
    AtomicBoolean m_lastRequest;

protected:
    MockChannelPutGet(Channel::shared_pointer const & channel,
                      ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
                      PVStructure::shared_pointer const & pvStructure,
                      PVStructure::shared_pointer const & pvRequest) :
        m_channel(channel),
        m_channelPutGetRequester(channelPutGetRequester),
        m_getStructure(getRequestedStructure(pvStructure, pvRequest, "getField")),
        m_getBitSet(new BitSet(m_getStructure->getNumberFields())),
        m_putStructure(getRequestedStructure(pvStructure, pvRequest, "putField")),
        m_putBitSet(new BitSet(m_putStructure->getNumberFields())),
        m_channelProcess(getChannelProcess(channel, pvRequest))

    {
        // always all
        m_getBitSet->set(0);
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
                static_cast<MockChannelPutGet*>(thisPtr.get())->m_putStructure->getStructure(),
                static_cast<MockChannelPutGet*>(thisPtr.get())->m_getStructure->getStructure());

        return thisPtr;
    }

    virtual ~MockChannelPutGet()
    {
    }

    virtual void putGet(PVStructure::shared_pointer const & pvPutStructure, BitSet::shared_pointer const & putBitSet)
    {
        // TODO use putBitSet and do not copy all
        // (note that server code has already not deserialized fields whose bits are not set)
        if (putBitSet->cardinality())
        {
            lock();
            m_putStructure->copyUnchecked(*pvPutStructure);
            unlock();
        }

        if (m_channelProcess)
            m_channelProcess->process();

        m_channelPutGetRequester->putGetDone(Status::Ok, shared_from_this(), m_getStructure, m_getBitSet);

        notifyStructureChanged(m_channel->getChannelName());

        if (m_lastRequest.get())
            destroy();
    }

    virtual void getGet()
    {
        m_channelPutGetRequester->getGetDone(Status::Ok, shared_from_this(), m_getStructure, m_getBitSet);

        if (m_lastRequest.get())
            destroy();
    }

    virtual void getPut()
    {
        // putGet might mess with bitSet
        m_putBitSet->clear();
        m_putBitSet->set(0);

        m_channelPutGetRequester->getPutDone(Status::Ok, shared_from_this(), m_putStructure, m_putBitSet);

        if (m_lastRequest.get())
            destroy();
    }

    virtual void lastRequest()
    {
        m_lastRequest.set();
    }

    virtual Channel::shared_pointer getChannel()
    {
        return m_channel;
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
    ChannelRPC::shared_pointer const & channelRPC,
    ChannelRPCRequester::shared_pointer const & channelRPCRequester,
    string const & helpText
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
                    "epics:nt/NTScalar:1.0", fieldNames, fields)
            )
        );

        static_pointer_cast<PVString>(result->getSubField<PVString>("value"))->put(helpText);
        channelRPCRequester->requestDone(Status::Ok, channelRPC, result);
        return true;
    }
    else
    {
        return false;
    }
}


class MockChannelRPC :
    public ChannelRPC,
    public std::tr1::enable_shared_from_this<ChannelRPC>
{
private:
    ChannelRPCRequester::shared_pointer m_channelRPCRequester;
    Channel::shared_pointer m_channel;
    PVStructure::shared_pointer m_pvStructure;
    AtomicBoolean m_lastRequest;

protected:
    MockChannelRPC(ChannelRPCRequester::shared_pointer const & channelRPCRequester,
                   Channel::shared_pointer const & channel, PVStructure::shared_pointer const & pvStructure,
                   PVStructure::shared_pointer const & /*pvRequest*/) :
        m_channelRPCRequester(channelRPCRequester), m_channel(channel), m_pvStructure(pvStructure)
    {
    }

public:
    static ChannelRPC::shared_pointer create(ChannelRPCRequester::shared_pointer const & channelRPCRequester,
            Channel::shared_pointer const & channel,
            PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        ChannelRPC::shared_pointer thisPtr(new MockChannelRPC(channelRPCRequester, channel, pvStructure, pvRequest));
        // TODO pvRequest
        channelRPCRequester->channelRPCConnect(Status::Ok, thisPtr);
        return thisPtr;
    }

    virtual ~MockChannelRPC()
    {
    }

    virtual void request(epics::pvData::PVStructure::shared_pointer const & pvArgument)
    {
        string channelName = m_channel->getChannelName();

        if (channelName == "testNTURI")
        {
            const string helpText =
                "Returns the NTURI structure response identical the NTURI request.\n"
                "Arguments: (none)\n";
            if (handleHelp(pvArgument, shared_from_this(), m_channelRPCRequester, helpText))
                return;

            if (!starts_with(pvArgument->getStructure()->getID(), "epics:nt/NTURI:1."))
            {
                PVStructure::shared_pointer nullPtr;
                Status errorStatus(Status::STATUSTYPE_ERROR, "argument is not a NTURI structure");
                m_channelRPCRequester->requestDone(errorStatus, shared_from_this(), nullPtr);
            }
            else
            {
                // return argument as result
                m_channelRPCRequester->requestDone(Status::Ok, shared_from_this(), pvArgument);
            }
        }

        // handle NTURI request
        PVStructure::shared_pointer args(
            (starts_with(pvArgument->getStructure()->getID(), "epics:nt/NTURI:1.")) ?
            pvArgument->getSubField<PVStructure>("query") :
            pvArgument
        );

        if (channelName == "testNTTable")
        {
            const string helpText =
                "Generates a NTTable structure response with 10 rows and a specified number of columns.\n"
                "Columns are labeled 'column<num>' and values are '<num> + random [0..1)'.\n"
                "Arguments:\n\tstring columns\tnumber of table columns\n";
            if (handleHelp(args, shared_from_this(), m_channelRPCRequester, helpText))
                return;

            PVStringPtr columns = dynamic_pointer_cast<PVString>(args->getSubField("columns"));
            if (columns.get() == 0)
            {
                PVStructure::shared_pointer nullPtr;
                Status errorStatus(Status::STATUSTYPE_ERROR, "no string 'columns' argument specified");
                m_channelRPCRequester->requestDone(errorStatus, shared_from_this(), nullPtr);
            }
            else
            {
                int columnsCount = atoi(columns->get().c_str());
                PVStructure::shared_pointer result = createNTTable(columnsCount);
                generateNTTableDoubleValues(result);
                m_channelRPCRequester->requestDone(Status::Ok, shared_from_this(), result);
            }
        }
        else if (channelName == "testNTNameValue")
        {
            const string helpText =
                "Generates a NTNameValue structure response with a specified number of columns.\n"
                "Columns are labeled 'name<num>' and values are '<num> + random [0..1)'.\n"
                "Arguments:\n\tstring columns\tnumber of columns\n";
            if (handleHelp(args, shared_from_this(), m_channelRPCRequester, helpText))
                return;

            PVStringPtr columns = dynamic_pointer_cast<PVString>(args->getSubField("columns"));
            if (columns.get() == 0)
            {
                PVStructure::shared_pointer nullPtr;
                Status errorStatus(Status::STATUSTYPE_ERROR, "no string 'columns' argument specified");
                m_channelRPCRequester->requestDone(errorStatus, shared_from_this(), nullPtr);
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
                            "epics:nt/NTNameValue:1.0", tableFieldNames, tableFields)
                    )
                );
                result->getSubField<PVStringArray>("name")->replace(freeze(labels));

                int32 len = columnsCount;
                PVDoubleArray::svector mv(len);
                for (int r = 0; r < len; r++)
                    mv[r] = rand()/((double)RAND_MAX+1) + (int)(r);
                result->getSubField<PVDoubleArray>("value")->replace(freeze(mv));

                m_channelRPCRequester->requestDone(Status::Ok, shared_from_this(), result);
            }
        }
        else if (channelName == "testNTMatrix")
        {
            const string helpText =
                "Generates a NTMatrix structure response with a specified number of rows and columns.\n"
                "Matrix values are '<row> + random [0..1)'.\n"
                "Arguments:\n"
                "\tstring rows\tnumber of matrix rows\n"
                "\tstring columns\tnumber of matrix columns\n"
                "\t[string bycolumns\torder matrix values in a column-major order]\n";
            if (handleHelp(args, shared_from_this(), m_channelRPCRequester, helpText))
                return;

            PVStringPtr rows = dynamic_pointer_cast<PVString>(args->getSubField("rows"));
            PVStringPtr columns = dynamic_pointer_cast<PVString>(args->getSubField("columns"));
            if (rows.get() == 0 || columns.get() == 0)
            {
                PVStructure::shared_pointer nullPtr;
                Status errorStatus(Status::STATUSTYPE_ERROR, "no string 'rows' and 'columns' arguments specified");
                m_channelRPCRequester->requestDone(errorStatus, shared_from_this(), nullPtr);
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
                        getFieldCreate()->createStructure("epics:nt/NTMatrix:1.0", fieldNames, fields)
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

                m_channelRPCRequester->requestDone(Status::Ok, shared_from_this(), result);
            }
        }
        else if (channelName.find("testImage") == 0)
        {
            const string helpText =
                "Generates a NTNDArray structure response that has encoded a specified image.\n"
                "Arguments:\n"
                "\tstring file\tfile path (relative to a location where the server was started) of a raw encoded image.\n"
                "\t\t\tTwo image types are supported:\n"
                "\t\t\t\t- RGB888 encoded (file extension '.rgb')\n"
                "\t\t\t\t- 8-bit grayscale encoded (any other extension).\n"
                "\t\t\tTo generate such encoded images you can use ImageMagick 'convert' tool, e.g.:\n"
                "\t\t\t\tconvert my_image.png my_image.rgb\n"
                "\tstring w\timage width\n"
                "\tstring h\timage height\n";
            if (handleHelp(args, shared_from_this(), m_channelRPCRequester, helpText))
                return;

            PVStringPtr file = dynamic_pointer_cast<PVString>(args->getSubField("file"));
            PVStringPtr w = dynamic_pointer_cast<PVString>(args->getSubField("w"));
            PVStringPtr h = dynamic_pointer_cast<PVString>(args->getSubField("h"));
            if (file.get() == 0 || w.get() == 0 || h.get() == 0)
            {
                PVStructure::shared_pointer nullPtr;
                Status errorStatus(Status::STATUSTYPE_ERROR, "not all 'file', 'w' and 'h' arguments specified");
                m_channelRPCRequester->requestDone(errorStatus, shared_from_this(), nullPtr);
            }
            else
            {
                int32 wv = atoi(w->get().c_str());
                int32 hv = atoi(h->get().c_str());
                string filev = file->get();

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
                            initImage(m_pvStructure, "", 2 /* NDColorModeRGB1=2 */, 3, dim, fileSize, 0);
                        }
                        else
                        {
                            const int32_t dim[] = { wv, hv };
                            initImage(m_pvStructure, "", 0 /* NDColorModeMono=0 */, 2, dim, fileSize, 0);
                        }

                        PVUnionPtr unionValue = m_pvStructure->getSubField<PVUnion>("value");
                        PVByteArrayPtr value = unionValue->select<PVByteArray>("byteValue");
                        PVByteArray::svector temp(value->reuse());
                        temp.resize(fileSize);
                        in.readsome((char*)temp.data(), fileSize);
                        value->replace(freeze(temp));

                        m_channelRPCRequester->requestDone(Status::Ok, shared_from_this(), m_pvStructure);

                        // for monitors
                        notifyStructureChanged(channelName);
                    }
                    else
                    {
                        PVStructure::shared_pointer nullPtr;
                        Status errorStatus(Status::STATUSTYPE_ERROR, "file size does not match given 'w' and 'h'");
                        m_channelRPCRequester->requestDone(errorStatus, shared_from_this(), nullPtr);
                    }
                }
                else
                {
                    PVStructure::shared_pointer nullPtr;
                    Status errorStatus(Status::STATUSTYPE_ERROR, "failed to open image file specified");
                    m_channelRPCRequester->requestDone(errorStatus, shared_from_this(), nullPtr);
                }
            }
        }
        else if (channelName == "testSum") {

            const string helpText =
                "Calculates a sum of two integer values.\n"
                "Arguments:\n"
                "\tint a\tfirst integer number\n"
                "\tint b\tsecond integer number\n";
            if (handleHelp(args, shared_from_this(), m_channelRPCRequester, helpText))
                return;

            PVInt::shared_pointer pa = args->getSubField<PVInt>("a");
            PVInt::shared_pointer pb = args->getSubField<PVInt>("b");
            if (!pa || !pb)
            {
                PVStructure::shared_pointer nullPtr;
                Status errorStatus(Status::STATUSTYPE_ERROR, "int a and int b arguments are required");
                m_channelRPCRequester->requestDone(errorStatus, shared_from_this(), nullPtr);
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
            result->getSubField<PVInt>("c")->put(a+b);

            m_channelRPCRequester->requestDone(Status::Ok, shared_from_this(), result);

        }
#ifndef TESTSERVERNOMAIN
        else if (channelName.find("testServerShutdown") == 0)
        {
            PVStructure::shared_pointer nullPtr;
            m_channelRPCRequester->requestDone(Status::Ok, shared_from_this(), nullPtr);
            testServerShutdown();
        }
#endif
        else
        {
            /*
            std::string s;
            pvArgument->toString(&s);
            std::cout << "RPC" << std::endl << s << std::endl;
            */
            m_channelRPCRequester->requestDone(Status::Ok, shared_from_this(), m_pvStructure);
        }

        if (m_lastRequest.get())
            destroy();
    }

    virtual void lastRequest()
    {
        m_lastRequest.set();
    }

    virtual Channel::shared_pointer getChannel()
    {
        return m_channel;
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







class MockChannelArray :
    public ChannelArray,
    public std::tr1::enable_shared_from_this<ChannelArray>
{
private:
    Channel::shared_pointer m_channel;
    ChannelArrayRequester::shared_pointer m_channelArrayRequester;
    PVArray::shared_pointer m_pvArray;
    PVArray::shared_pointer m_pvStructureArray;
    AtomicBoolean m_lastRequest;

protected:
    MockChannelArray(Channel::shared_pointer const & channel,
                     ChannelArrayRequester::shared_pointer const & channelArrayRequester,
                     PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & /*pvRequest*/) :
        m_channel(channel), m_channelArrayRequester(channelArrayRequester)
    {
        m_pvStructureArray = pvStructure->getSubField<PVArray>("value");
        if (m_pvStructureArray.get())
            m_pvArray = std::tr1::dynamic_pointer_cast<PVArray>(
                            getPVDataCreate()->createPVField(m_pvStructureArray->getField()));
    }

public:
    static ChannelArray::shared_pointer create(Channel::shared_pointer const & channel,
            ChannelArrayRequester::shared_pointer const & channelArrayRequester,
            PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        ChannelArray::shared_pointer thisPtr(new MockChannelArray(channel, channelArrayRequester, pvStructure, pvRequest));

        PVArray::shared_pointer array(static_cast<MockChannelArray*>(thisPtr.get())->m_pvArray);
        if (array.get())
            channelArrayRequester->channelArrayConnect(Status::Ok, thisPtr, array->getArray());
        else
        {
            Status errorStatus(Status::STATUSTYPE_ERROR, "no 'value' subfield of array type");
            channelArrayRequester->channelArrayConnect(errorStatus, thisPtr, Array::const_shared_pointer());
        }

        return thisPtr;
    }

    virtual ~MockChannelArray()
    {
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

    virtual void putArray(PVArray::shared_pointer const & pvArray, size_t offset, size_t count, size_t stride)
    {
        // TODO stride support
        if (stride == 1)
        {

            size_t len = pvArray->getLength();

            size_t o = offset < len ? offset : len;
            size_t c = count;
            if (c == 0 || ((o + c) > len)) c = len - o;

            Field::const_shared_pointer field = pvArray->getField();
            Type type = field->getType();
            if (type == scalarArray)
            {
                switch (std::tr1::static_pointer_cast<const ScalarArray>(field)->getElementType())
                {
                case pvBoolean:
                    put<PVBooleanArray>(pvArray, m_pvStructureArray, o, c);
                    break;
                case pvByte:
                    put<PVByteArray>(pvArray, m_pvStructureArray, o, c);
                    break;
                case pvShort:
                    put<PVShortArray>(pvArray, m_pvStructureArray, o, c);
                    break;
                case pvInt:
                    put<PVIntArray>(pvArray, m_pvStructureArray, o, c);
                    break;
                case pvLong:
                    put<PVLongArray>(pvArray, m_pvStructureArray, o, c);
                    break;
                case pvUByte:
                    put<PVUByteArray>(pvArray, m_pvStructureArray, o, c);
                    break;
                case pvUShort:
                    put<PVUShortArray>(pvArray, m_pvStructureArray, o, c);
                    break;
                case pvUInt:
                    put<PVUIntArray>(pvArray, m_pvStructureArray, o, c);
                    break;
                case pvULong:
                    put<PVULongArray>(pvArray, m_pvStructureArray, o, c);
                    break;
                case pvFloat:
                    put<PVFloatArray>(pvArray, m_pvStructureArray, o, c);
                    break;
                case pvDouble:
                    put<PVDoubleArray>(pvArray, m_pvStructureArray, o, c);
                    break;
                case pvString:
                    put<PVStringArray>(pvArray, m_pvStructureArray, o, c);
                    break;
                }
            }
            else if (type == structureArray)
                put<PVStructureArray>(pvArray, m_pvStructureArray, o, c);
            else if (type == unionArray)
                put<PVUnionArray>(pvArray, m_pvStructureArray, o, c);

            m_channelArrayRequester->putArrayDone(Status::Ok, shared_from_this());
        }
        else
        {
            Status notSupported(Status::STATUSTYPE_ERROR, "stride != 1 is not supported");
            m_channelArrayRequester->putArrayDone(notSupported, shared_from_this());
        }

        if (m_lastRequest.get())
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


    virtual void getArray(size_t offset, size_t count, size_t stride)
    {
        // TODO stride support
        if (stride == 1)
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
                case pvBoolean:
                    get<PVBooleanArray>(m_pvStructureArray, m_pvArray, o, c);
                    break;
                case pvByte:
                    get<PVByteArray>(m_pvStructureArray, m_pvArray, o, c);
                    break;
                case pvShort:
                    get<PVShortArray>(m_pvStructureArray, m_pvArray, o, c);
                    break;
                case pvInt:
                    get<PVIntArray>(m_pvStructureArray, m_pvArray, o, c);
                    break;
                case pvLong:
                    get<PVLongArray>(m_pvStructureArray, m_pvArray, o, c);
                    break;
                case pvUByte:
                    get<PVUByteArray>(m_pvStructureArray, m_pvArray, o, c);
                    break;
                case pvUShort:
                    get<PVUShortArray>(m_pvStructureArray, m_pvArray, o, c);
                    break;
                case pvUInt:
                    get<PVUIntArray>(m_pvStructureArray, m_pvArray, o, c);
                    break;
                case pvULong:
                    get<PVULongArray>(m_pvStructureArray, m_pvArray, o, c);
                    break;
                case pvFloat:
                    get<PVFloatArray>(m_pvStructureArray, m_pvArray, o, c);
                    break;
                case pvDouble:
                    get<PVDoubleArray>(m_pvStructureArray, m_pvArray, o, c);
                    break;
                case pvString:
                    get<PVStringArray>(m_pvStructureArray, m_pvArray, o, c);
                    break;
                }
            }
            else if (type == structureArray)
                get<PVStructureArray>(m_pvStructureArray, m_pvArray, o, c);
            else if (type == unionArray)
                get<PVUnionArray>(m_pvStructureArray, m_pvArray, o, c);

            m_channelArrayRequester->getArrayDone(Status::Ok, shared_from_this(), m_pvArray);
        }
        else
        {
            Status notSupported(Status::STATUSTYPE_ERROR, "stride != 1 is not supported");
            m_channelArrayRequester->putArrayDone(notSupported, shared_from_this());
        }

        if (m_lastRequest.get())
            destroy();
    }

    virtual void setLength(size_t length)
    {
        m_pvStructureArray->setLength(length);

        m_channelArrayRequester->setLengthDone(Status::Ok, shared_from_this());

        if (m_lastRequest.get())
            destroy();
    }

    virtual void getLength()
    {

        m_channelArrayRequester->getLengthDone(Status::Ok, shared_from_this(),
                                               m_pvStructureArray->getLength());

        if (m_lastRequest.get())
            destroy();
    }

    virtual void lastRequest()
    {
        m_lastRequest.set();
    }

    virtual Channel::shared_pointer getChannel()
    {
        return m_channel;
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

#include <pv/current_function.h>

class TraceLog {
public:

    TraceLog(const std::string &method) : m_method(method) {
        std::cout << "--> " << m_method << std::endl;
    }

    ~TraceLog() {
        std::cout << "<-- " << m_method << std::endl;
    }

private:
    std::string m_method;
};




class MockMonitor :
    public Monitor,
    public StructureChangedCallback,
    public std::tr1::enable_shared_from_this<MockMonitor>
{
private:
    string m_channelName;
    MonitorRequester::shared_pointer m_monitorRequester;
    bool m_continuous;
    PVStructure::shared_pointer m_pvStructure;
    PVStructure::shared_pointer m_ccopy;
    Mutex m_lock;
    enum QueueState { MM_STATE_FULL, MM_STATE_TAKEN, MM_STATE_FREE };
    QueueState m_state ;
    AtomicBoolean m_active;


    MonitorElement::shared_pointer m_thisPtr;

protected:
    MockMonitor(std::string const & channelName, MonitorRequester::shared_pointer const & monitorRequester,
                PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest) :
        m_channelName(channelName),
        m_monitorRequester(monitorRequester),
        m_continuous(false),
        m_pvStructure(getRequestedStructure(pvStructure, pvRequest)),
        m_ccopy(getPVDataCreate()->createPVStructure(m_pvStructure->getStructure())),
        m_lock(),
        m_state(MM_STATE_FREE),
        m_thisPtr(new MonitorElement(m_ccopy))
    {
        PVScalar::shared_pointer pvScalar = pvRequest->getSubField<PVScalar>("record._options.velocious");
        if (pvScalar)
            m_continuous = pvScalar->getAs<epics::pvData::boolean>();

        // we always send all
        m_thisPtr->changedBitSet->set(0);
    }

public:
    static Monitor::shared_pointer create(std::string const & channelName,
                                          MonitorRequester::shared_pointer const & monitorRequester,
                                          PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        // TODO use std::make_shared
        std::tr1::shared_ptr<MockMonitor> tp(
            new MockMonitor(channelName, monitorRequester, pvStructure, pvRequest)
        );
        Monitor::shared_pointer thisPtr = tp;

        // register
        structureChangedListeners[channelName].push_back(std::tr1::dynamic_pointer_cast<StructureChangedCallback>(thisPtr));

        StructureConstPtr structurePtr = static_cast<MockMonitor*>(thisPtr.get())->m_pvStructure->getStructure();
        monitorRequester->monitorConnect(Status::Ok, thisPtr, structurePtr);

        return thisPtr;
    }

    virtual ~MockMonitor()
    {
    }

    void copy()
    {
        {
            lock();
            m_ccopy->copyUnchecked(*m_pvStructure);
            unlock();
        }
    }
    virtual Status start()
    {

        {
            Lock xx(m_lock);
            m_state = MM_STATE_FULL;
            copy();
        }

        // first monitor
        Monitor::shared_pointer thisPtr = shared_from_this();
        m_monitorRequester->monitorEvent(thisPtr);

        m_active.set();   // set here not to have race condition on first monitor

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

                if (m_state == MM_STATE_FULL || m_state == MM_STATE_TAKEN)      // "queue" full
                {
                    m_thisPtr->overrunBitSet->set(0);
                    copy();
                    return;
                }
                else
                {
                    m_thisPtr->overrunBitSet->clear(0);
                    m_state = MM_STATE_FULL;
                    copy();
                }
            }

            Monitor::shared_pointer thisPtr = shared_from_this();
            m_monitorRequester->monitorEvent(thisPtr);
        }
    }

    virtual MonitorElement::shared_pointer poll()
    {
        Lock xx(m_lock);
        if (m_state != MM_STATE_FULL)
        {
            return MonitorElement::shared_pointer();
        }
        else
        {
            m_state = MM_STATE_TAKEN;
            return m_thisPtr;
        }
    }

    virtual void release(MonitorElement::shared_pointer const & /*monitorElement*/)
    {
        Lock xx(m_lock);
        if (m_state == MM_STATE_TAKEN)
        {
            if (m_continuous)
                m_state = MM_STATE_FULL;
            else
                m_state = MM_STATE_FREE;
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


class MockChannel :
    public Channel,
    public std::tr1::enable_shared_from_this<MockChannel>
{
private:
    ChannelProvider::weak_pointer  m_provider;
    ChannelRequester::shared_pointer m_requester;
    string m_name;
    string m_remoteAddress;
public: // TODO
    PVStructure::shared_pointer m_pvStructure;

protected:

    MockChannel(
        ChannelProvider::shared_pointer provider,
        ChannelRequester::shared_pointer requester,
        string name,
        string remoteAddress) :
        m_provider(provider),
        m_requester(requester),
        m_name(name),
        m_remoteAddress(remoteAddress),
        m_pvStructure()
    {
        if (structureStore.find(m_name) != structureStore.end())
            m_pvStructure = structureStore[m_name];

        else
        {
            // create structure

            if (m_name.find("testArray") == 0)
            {
                string allProperties("");
                //            string allProperties("alarm,timeStamp,display,control");
                m_pvStructure = getStandardPVField()->scalarArray(pvDouble,allProperties);
                PVDoubleArrayPtr pvField = m_pvStructure->getSubField<PVDoubleArray>("value");

                int specCount = 0;
                char postfix[64];
                int done = sscanf(m_name.c_str(), "testArray%d%s", &specCount, postfix);

                if (done && specCount > 0)
                {
                    PVDoubleArray::svector temp(pvField->reuse());
                    temp.resize(specCount);

                    double v = 0;
                    for (int n = 0; n < specCount; n++)
                    {
                        temp[n] = v;
                        v+=1;
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
                        temp[n] = v;
                        v+=1.1;
                    }
                    pvField->replace(freeze(temp));
                }
                /*
                printf("array prepared------------------------------------!!!\n");
                string str;
                pvField->toString(&str);
                printf("%s\n", str.c_str());
                printf("=============------------------------------------!!!\n");
                */
            }
            else if (m_name.find("testMP") == 0
                     || m_name.find("testImage") == 0)
            {
                m_pvStructure = getPVDataCreate()->createPVStructure(createNTNDArrayStructure());
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
                        getFieldCreate()->createStructure("epics:nt/NTMatrix:1.0", fieldNames, fields)
                    );

                // fill with default values
                PVIntArray::svector dimValue(1);
                dimValue[0] = 0;
                m_pvStructure->getSubField<PVIntArray>("dim")->replace(freeze(dimValue));

                m_pvStructure->getSubField<PVString>("descriptor")->put("Simulated ADC that provides NTMatrix value");
                PVStructurePtr displayStructure = m_pvStructure->getSubField<PVStructure>("display");
                displayStructure->getSubField<PVDouble>("limitLow")->put(-1.0);
                displayStructure->getSubField<PVDouble>("limitHigh")->put(1.0);
                displayStructure->getSubField<PVString>("description")->put("Simulated ADC");
                displayStructure->getSubField<PVString>("units")->put("V");
            }
            else if (m_name.find("testRPC") == 0 || m_name == "testNTTable" || m_name == "testNTMatrix")
            {
                StringArray fieldNames;
                PVFieldPtrArray fields;
                m_pvStructure = getPVDataCreate()->createPVStructure(fieldNames, fields);
            }
            else if (m_name.find("testValueOnly") == 0)
            {
                string allProperties("");
                m_pvStructure = getStandardPVField()->scalar(pvDouble,allProperties);
            }
            else if (m_name == "testCounter" || m_name == "testSimpleCounter")
            {
                string allProperties("timeStamp");
                m_pvStructure = getStandardPVField()->scalar(pvInt,allProperties);
            }
            else if (m_name == "testBinaryCounter" )
            {
                epics::pvData::StructureConstPtr s =
                    getFieldCreate()->createFieldBuilder()->
                    setId("epics:test/binaryCounter:1.0")->
                    add("value", pvByte)->
                    add("bit0", pvBoolean)->
                    add("bit1", pvBoolean)->
                    add("bit2", pvBoolean)->
                    add("bit3", pvBoolean)->
                    add("bit4", pvBoolean)->
                    add("bit5", pvBoolean)->
                    add("bit6", pvBoolean)->
                    add("bit7", pvBoolean)->
                    add("timeStamp", getStandardField()->timeStamp())->
                    createStructure();
                m_pvStructure = getPVDataCreate()->createPVStructure(s);
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
                string allProperties("timeStamp");
                m_pvStructure = getStandardPVField()->enumerated(choices,allProperties);
            }
            else if (m_name == "testBoundedString" )
            {
                epics::pvData::StructureConstPtr s =
                    getFieldCreate()->createFieldBuilder()->
                    addBoundedString("value", 8)->
                    add("timeStamp", getStandardField()->timeStamp())->
                    createStructure();
                m_pvStructure = getPVDataCreate()->createPVStructure(s);
            }
            else if (m_name == "testBoundedArray" )
            {
                epics::pvData::StructureConstPtr s =
                    getFieldCreate()->createFieldBuilder()->
                    addBoundedArray("value", pvDouble, 8)->
                    add("timeStamp", getStandardField()->timeStamp())->
                    createStructure();
                m_pvStructure = getPVDataCreate()->createPVStructure(s);
            }
            else if (m_name == "testFixedArray" )
            {
                epics::pvData::StructureConstPtr s =
                    getFieldCreate()->createFieldBuilder()->
                    addFixedArray("value", pvDouble, 8)->
                    add("timeStamp", getStandardField()->timeStamp())->
                    createStructure();
                m_pvStructure = getPVDataCreate()->createPVStructure(s);
                m_pvStructure->getSubField<PVArray>("value")->setLength(8);
            }
            else if (m_name == "testStructureArray" )
            {
                epics::pvData::StructureConstPtr s =
                    getFieldCreate()->createFieldBuilder()->
                    add("value", getFieldCreate()->createStructureArray(getStandardField()->alarm()))->
                    createStructure();
                m_pvStructure = getPVDataCreate()->createPVStructure(s);

                PVStructureArray::svector data(5);
                data[1] = getPVDataCreate()->createPVStructure(getStandardField()->alarm());
                data[4] = getPVDataCreate()->createPVStructure(getStandardField()->alarm());

                m_pvStructure->getSubField<PVStructureArray>("value")->replace(freeze(data));
            }
            else
            {
                string allProperties("alarm,timeStamp,display,control,valueAlarm");
                m_pvStructure = getStandardPVField()->scalar(pvDouble,allProperties);
                //PVDoublePtr pvField = m_pvStructure->getSubField<PVDouble>(std::string("value"));
                //pvField->put(1.123);
            }

            structureStore[m_name] = m_pvStructure;
        }
    }

public:

    static Channel::shared_pointer create(
        ChannelProvider::shared_pointer provider,
        ChannelRequester::shared_pointer requester,
        string name,
        string remoteAddress)
    {
        // TODO use std::make_shared
        std::tr1::shared_ptr<MockChannel> tp(
            new MockChannel(provider, requester, name, remoteAddress)
        );
        Channel::shared_pointer channelPtr = tp;
        // already connected, report state
        requester->channelStateChange(channelPtr, CONNECTED);

        return channelPtr;
    }

    virtual ~MockChannel()
    {}

    virtual void destroy()
    {
    };

    virtual string getRequesterName()
    {
        return getChannelName();
    };

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual ChannelProvider::shared_pointer getProvider()
    {
        return m_provider.lock();
    }

    virtual std::string getRemoteAddress()
    {
        return m_remoteAddress;
    }

    virtual std::string getChannelName()
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

    virtual void getField(GetFieldRequester::shared_pointer const & requester,std::string const & subField)
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

        if(!pvField.get())
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
        return MockChannelProcess::create(shared_from_this(), channelProcessRequester, m_pvStructure, pvRequest);
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
        return MockChannelRPC::create(channelRPCRequester, shared_from_this(), m_pvStructure, pvRequest);
    }

    virtual Monitor::shared_pointer createMonitor(
        MonitorRequester::shared_pointer const & monitorRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
        return MockMonitor::create(m_name, monitorRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelArray::shared_pointer createChannelArray(
        ChannelArrayRequester::shared_pointer const & channelArrayRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
        return MockChannelArray::create(shared_from_this(), channelArrayRequester, m_pvStructure, pvRequest);
    }

    virtual void printInfo() {
        printInfo(std::cout);
    }

    virtual void printInfo(std::ostream& out) {
        out << "CHANNEL  : " << getChannelName() << std::endl;

        ConnectionState state = getConnectionState();
        out << "STATE    : " << ConnectionStateNames[state] << std::endl;
        if (state == CONNECTED)
        {
            out << "ADDRESS  : " << getRemoteAddress() << std::endl;
            //out << "RIGHTS   : " << getAccessRights() << std::endl;
        }
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

    virtual void cancel()
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

    static const string PROVIDER_NAME;

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

        c = MockChannel::create(chProviderPtr, cr, "testBinaryCounter", "local");
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

    virtual std::string getProviderName()
    {
        return PROVIDER_NAME;
    }

    virtual void destroy()
    {
    }

    virtual ChannelFind::shared_pointer channelFind(
        std::string const & channelName,
        ChannelFindRequester::shared_pointer const & channelFindRequester)
    {
        // channel that starts with "test" always exists
        bool exists = (channelName.find("test") == 0);
        channelFindRequester->channelFindResult(Status::Ok, m_mockChannelFind, exists);
        return m_mockChannelFind;
    }

    virtual ChannelFind::shared_pointer channelList(
        ChannelListRequester::shared_pointer const & channelListRequester)
    {
        if (!channelListRequester.get())
            throw std::runtime_error("null requester");

        // NOTE: this adds only active channels, not all (especially RPC ones)
        PVStringArray::svector channelNames;
        {
            Lock guard(structureStoreMutex);
            channelNames.reserve(structureStore.size());
            for (map<string, PVStructure::shared_pointer>::const_iterator iter = structureStore.begin();
                    iter != structureStore.end();
                    iter++)
                channelNames.push_back(iter->first);
        }
        channelListRequester->channelListResult(Status::Ok, m_mockChannelFind, freeze(channelNames), true);
        return m_mockChannelFind;
    }

    virtual Channel::shared_pointer createChannel(
        std::string const & channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        short priority)
    {
        return createChannel(channelName, channelRequester, priority, "local");
    }

    virtual Channel::shared_pointer createChannel(
        std::string const & channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        short /*priority*/,
        std::string const & address)
    {
        if (address == "local")
        {
            // this is a server instance provider, address holds remote socket IP
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
    epics::auto_ptr<epics::pvData::Thread> m_scan1HzThread;

    ProcessAction m_scan10Hz;
    epics::auto_ptr<epics::pvData::Thread> m_scan10HzThread;

    ADCAction m_adcAction;
    epics::auto_ptr<epics::pvData::Thread> m_adcThread;

    NTNDArrayAction m_imgAction;
    epics::auto_ptr<epics::pvData::Thread> m_imgThread;
};

const string MockServerChannelProvider::PROVIDER_NAME = "local";

struct TestServer
{
    POINTER_DEFINITIONS(TestServer);

    static TestServer::shared_pointer ctx;

    ServerContext::shared_pointer context;

    TestServer(const epics::pvAccess::Configuration::shared_pointer& conf)
    {
        std::tr1::shared_ptr<MockServerChannelProvider> prov(new MockServerChannelProvider);
        prov->initialize();
        context = ServerContext::create(ServerContext::Config()
                                        .config(conf)
                                        .provider(prov));
    }

    ~TestServer()
    {
        context->shutdown();

        structureChangedListeners.clear();
        {
            Lock guard(structureStoreMutex);
            structureStore.clear();
        }
        ctx.reset();

        shutdownSimADCs();
    }
    // Use with EPICS_PVA_SERVER_PORT==0 for dynamic port (unit-tests)
    unsigned short getServerPort()
    {
        return context->getServerPort();
    }
    unsigned short getBroadcastPort()
    {
        return context->getBroadcastPort();
    }

    void waitForShutdown() {
        context->shutdown();
    }
    void shutdown() {
        context->shutdown();
    }
};

TestServer::shared_pointer TestServer::ctx;


void testServerShutdown()
{
    TestServer::ctx->shutdown();
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
    std::string timeToRun("0");

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */

    while ((opt = getopt(argc, argv, ":ht:dc")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage(argv);
            return 0;
        case 't':               /* Print usage */
            timeToRun = optarg;
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

    TestServer::shared_pointer srv(new TestServer(ConfigurationBuilder()
                                   .push_env()
                                   .add("timeToRun", timeToRun)
                                   .push_map()
                                   .build()));
    TestServer::ctx = srv;
    srv->context->printInfo();
    srv->context->run(epics::pvData::castUnsafe<epicsUInt32>(timeToRun));

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
