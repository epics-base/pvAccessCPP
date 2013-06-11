#include <iostream>
#include <pv/clientFactory.h>
#include <pv/pvAccess.h>

#include <stdio.h>
#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsThread.h>
#include <pv/logger.h>
#include <pv/lock.h>

#include <vector>
#include <string>
#include <sstream>

#include <pv/event.h>
#include <epicsExit.h>

#include "pvutils.cpp"
#include <pv/convert.h>

#include <pv/caProvider.h>

using namespace std;
using namespace std::tr1;
using namespace epics::pvData;
using namespace epics::pvAccess;

// TODO add >> operator support to PVField
// code copied from Convert.cpp and added error handling, and more strict boolean convert

void fromString(PVScalarPtr const & pvScalar, String const & from)
{
    ScalarConstPtr scalar = pvScalar->getScalar();
    ScalarType scalarType = scalar->getScalarType();
    switch(scalarType) {
        case pvBoolean: {
                PVBooleanPtr pv = static_pointer_cast<PVBoolean>(pvScalar);
                bool isTrue  = (from.compare("true")==0  || from.compare("1")==0);
                bool isFalse = (from.compare("false")==0 || from.compare("0")==0);
                if (!(isTrue || isFalse))
                	throw runtime_error("failed to parse field " + pvScalar->getFieldName() + " (boolean) from string value '" + from + "'");
                pv->put(isTrue == true);
                return;
            }
        case pvByte : {
                PVBytePtr pv = static_pointer_cast<PVByte>(pvScalar);
                int ival;
                int result = sscanf(from.c_str(),"%d",&ival);
                if (result != 1)
                	throw runtime_error("failed to parse field " + pvScalar->getFieldName() + " (byte) from string value '" + from + "'");
                int8 value = ival;
                pv->put(value);
                return;
            }
        case pvShort : {
                PVShortPtr pv = static_pointer_cast<PVShort>(pvScalar);
                int ival;
                int result = sscanf(from.c_str(),"%d",&ival);
                if (result != 1)
                	throw runtime_error("failed to parse field " + pvScalar->getFieldName() + " (short) from string value '" + from + "'");
                int16 value = ival;
                pv->put(value);
                return;
            }
        case pvInt : {
                PVIntPtr pv = static_pointer_cast<PVInt>(pvScalar);
                int ival;
                int result = sscanf(from.c_str(),"%d",&ival);
                if (result != 1)
                	throw runtime_error("failed to parse field " + pvScalar->getFieldName() + " (int) from string value '" + from + "'");
                int32 value = ival;
                pv->put(value);
                return;
            }
        case pvLong : {
                PVLongPtr pv = static_pointer_cast<PVLong>(pvScalar);
                int64 ival;
                int result = sscanf(from.c_str(),"%lld",(long long *)&ival);
                if (result != 1)
                	throw runtime_error("failed to parse field " + pvScalar->getFieldName() + " (long) from string value '" + from + "'");
                int64 value = ival;
                pv->put(value);
                return;
            }
        case pvUByte : {
                PVUBytePtr pv = static_pointer_cast<PVUByte>(pvScalar);
                unsigned int ival;
                int result = sscanf(from.c_str(),"%u",&ival);
                if (result != 1)
                	throw runtime_error("failed to parse field " + pvScalar->getFieldName() + " (ubyte) from string value '" + from + "'");
                uint8 value = ival;
                pv->put(value);
                return;
            }
        case pvUShort : {
                PVUShortPtr pv = static_pointer_cast<PVUShort>(pvScalar);
                unsigned int ival;
                int result = sscanf(from.c_str(),"%u",&ival);
                if (result != 1)
                	throw runtime_error("failed to parse field " + pvScalar->getFieldName() + " (ushort) from string value '" + from + "'");
                uint16 value = ival;
                pv->put(value);
                return;
            }
        case pvUInt : {
                PVUIntPtr pv = static_pointer_cast<PVUInt>(pvScalar);
                unsigned int ival;
                int result = sscanf(from.c_str(),"%u",&ival);
                if (result != 1)
                	throw runtime_error("failed to parse field " + pvScalar->getFieldName() + " (uint) from string value '" + from + "'");
                uint32 value = ival;
                pv->put(value);
                return;
            }
        case pvULong : {
                PVULongPtr pv = static_pointer_cast<PVULong>(pvScalar);
                unsigned long long ival;
                int result = sscanf(from.c_str(),"%llu",(long long unsigned int *)&ival);
                if (result != 1)
                	throw runtime_error("failed to parse field " + pvScalar->getFieldName() + " (ulong) from string value '" + from + "'");
                uint64 value = ival;
                pv->put(value);
                return;
            }
        case pvFloat : {
                PVFloatPtr pv = static_pointer_cast<PVFloat>(pvScalar);
                float value;
                int result = sscanf(from.c_str(),"%f",&value);
                if (result != 1)
                	throw runtime_error("failed to parse field " + pvScalar->getFieldName() + " (float) from string value '" + from + "'");
                pv->put(value);
                return;
            }
        case pvDouble : {
                PVDoublePtr pv = static_pointer_cast<PVDouble>(pvScalar);
                double value;
                int result = sscanf(from.c_str(),"%lf",&value);
                if (result != 1)
                	throw runtime_error("failed to parse field " + pvScalar->getFieldName() + " (double) from string value '" + from + "'");
                pv->put(value);
                return;
            }
        case pvString: {
                PVStringPtr value = static_pointer_cast<PVString>(pvScalar);
                value->put(from);
                return;
            }
    }
    String message("fromString unknown scalarType ");
    ScalarTypeFunc::toString(&message,scalarType);
    throw std::logic_error(message);
}


size_t convertFromStringArray(PVScalarArray *pv,
    size_t offset, size_t len,const StringArray & from, size_t fromOffset)
{
    ScalarType elemType = pv->getScalarArray()->getElementType();
    size_t ntransfered = 0;
    switch (elemType) {
    case pvBoolean: {
        PVBooleanArray *pvdata = static_cast<PVBooleanArray*>(pv);
        boolean data[1];
        while (len > 0) {
            String fromString = from[fromOffset];
            bool isTrue  = (fromString.compare("true")==0  || fromString.compare("1")==0);
            bool isFalse = (fromString.compare("false")==0 || fromString.compare("0")==0);
            if (!(isTrue || isFalse))
            {
            	char soffset[64];
            	sprintf(soffset, "%u", (unsigned int)offset);
            	throw runtime_error("failed to parse field " + pv->getFieldName() + " (boolean array at index " + soffset + ") from string value '" + fromString + "'");
            }
            data[0] = isTrue == true;
            if (pvdata->put(offset, 1, data, 0) == 0)
                return ntransfered;
            --len;
            ++ntransfered;
            ++offset;
            ++fromOffset;
        }
        return ntransfered;
    }
    case pvByte: {
        PVByteArray *pvdata = static_cast<PVByteArray*>(pv);
        int8 data[1];
        while (len > 0) {
            String fromString = from[fromOffset];
            int ival;
            int result = sscanf(fromString.c_str(),"%d",&ival);
            if (result != 1)
            {
            	char soffset[64];
            	sprintf(soffset, "%u", (unsigned int)offset);
            	throw runtime_error("failed to parse field " + pv->getFieldName() + " (byte array at index " + soffset + ") from string value '" + fromString + "'");
            }
            data[0] = ival;
            if (pvdata->put(offset, 1, data, 0) == 0)
                return ntransfered;
            --len;
            ++ntransfered;
            ++offset;
            ++fromOffset;
        }
        return ntransfered;
    }
    case pvShort: {
        PVShortArray *pvdata = static_cast<PVShortArray*>(pv);
        int16 data[1];
        while (len > 0) {
            String fromString = from[fromOffset];
            int ival;
            int result = sscanf(fromString.c_str(),"%d",&ival);
            if (result != 1)
            {
            	char soffset[64];
            	sprintf(soffset, "%u", (unsigned int)offset);
            	throw runtime_error("failed to parse field " + pv->getFieldName() + " (short array at index " + soffset + ") from string value '" + fromString + "'");
            }
            data[0] = ival;
            if (pvdata->put(offset, 1, data, 0) == 0)
                return ntransfered;
            --len;
            ++ntransfered;
            ++offset;
            ++fromOffset;
        }
        return ntransfered;
    }
    case pvInt: {
        PVIntArray *pvdata = static_cast<PVIntArray*>(pv);
        int32 data[1];
        while (len > 0) {
            String fromString = from[fromOffset];
            int ival;
            int result = sscanf(fromString.c_str(),"%d",&ival);
            if (result != 1)
            {
            	char soffset[64];
            	sprintf(soffset, "%u", (unsigned int)offset);
            	throw runtime_error("failed to parse field " + pv->getFieldName() + " (int array at index " + soffset + ") from string value '" + fromString + "'");
            }
            data[0] = ival;
            if (pvdata->put(offset, 1, data, 0) == 0)
                return ntransfered;
            --len;
            ++ntransfered;
            ++offset;
            ++fromOffset;
        }
        return ntransfered;
    }
    case pvLong: {
        PVLongArray *pvdata = static_cast<PVLongArray*>(pv);
        int64 data[1];
        while (len > 0) {
            String fromString = from[fromOffset];
            int64 ival;
            int result = sscanf(fromString.c_str(),"%lld",(long long int *)&ival);
            if (result != 1)
            {
            	char soffset[64];
            	sprintf(soffset, "%u", (unsigned int)offset);
            	throw runtime_error("failed to parse field " + pv->getFieldName() + " (long array at index " + soffset + ") from string value '" + fromString + "'");
            }
            data[0] = ival;
            if (pvdata->put(offset, 1, data, 0) == 0)
                return ntransfered;
            --len;
            ++ntransfered;
            ++offset;
            ++fromOffset;
        }
        return ntransfered;
    }
    case pvUByte: {
        PVUByteArray *pvdata = static_cast<PVUByteArray*>(pv);
        uint8 data[1];
        while (len > 0) {
            String fromString = from[fromOffset];
            unsigned int ival;
            int result = sscanf(fromString.c_str(),"%u",&ival);
            if (result != 1)
            {
            	char soffset[64];
            	sprintf(soffset, "%u", (unsigned int)offset);
            	throw runtime_error("failed to parse field " + pv->getFieldName() + " (ubyte array at index " + soffset + ") from string value '" + fromString + "'");
            }
            data[0] = ival;
            if (pvdata->put(offset, 1, data, 0) == 0)
                return ntransfered;
            --len;
            ++ntransfered;
            ++offset;
            ++fromOffset;
        }
        return ntransfered;
    }
    case pvUShort: {
        PVUShortArray *pvdata = static_cast<PVUShortArray*>(pv);
        uint16 data[1];
        while (len > 0) {
            String fromString = from[fromOffset];
            unsigned int ival;
            int result = sscanf(fromString.c_str(),"%u",&ival);
            if (result != 1)
            {
            	char soffset[64];
            	sprintf(soffset, "%u", (unsigned int)offset);
            	throw runtime_error("failed to parse field " + pv->getFieldName() + " (ushort array at index " + soffset + ") from string value '" + fromString + "'");
            }
            data[0] = ival;
            if (pvdata->put(offset, 1, data, 0) == 0)
                return ntransfered;
            --len;
            ++ntransfered;
            ++offset;
            ++fromOffset;
        }
        return ntransfered;
    }
    case pvUInt: {
        PVUIntArray *pvdata = static_cast<PVUIntArray*>(pv);
        uint32 data[1];
        while (len > 0) {
            String fromString = from[fromOffset];
            unsigned int ival;
            int result = sscanf(fromString.c_str(),"%u",&ival);
            if (result != 1)
            {
            	char soffset[64];
            	sprintf(soffset, "%u", (unsigned int)offset);
            	throw runtime_error("failed to parse field " + pv->getFieldName() + " (uint array at index " + soffset + ") from string value '" + fromString + "'");
            }
            data[0] = ival;
            if (pvdata->put(offset, 1, data, 0) == 0)
                return ntransfered;
            --len;
            ++ntransfered;
            ++offset;
            ++fromOffset;
        }
        return ntransfered;
    }
    case pvULong: {
        PVULongArray *pvdata = static_cast<PVULongArray*>(pv);
        uint64 data[1];
        while (len > 0) {
            String fromString = from[fromOffset];
            uint64 ival;
            int result = sscanf(fromString.c_str(),"%lld",(unsigned long long int *)&ival);
            if (result != 1)
            {
            	char soffset[64];
            	sprintf(soffset, "%u", (unsigned int)offset);
            	throw runtime_error("failed to parse field " + pv->getFieldName() + " (ulong array at index " + soffset + ") from string value '" + fromString + "'");
            }
            data[0] = ival;
            if (pvdata->put(offset, 1, data, 0) == 0)
                return ntransfered;
            --len;
            ++ntransfered;
            ++offset;
            ++fromOffset;
        }
        return ntransfered;
    }
    case pvFloat: {
        PVFloatArray *pvdata = static_cast<PVFloatArray*>(pv);
        float data[1];
        while (len > 0) {
            String fromString = from[fromOffset];
            float fval;
            int result = sscanf(fromString.c_str(),"%f",&fval);
            if (result != 1)
            {
            	char soffset[64];
            	sprintf(soffset, "%u", (unsigned int)offset);
            	throw runtime_error("failed to parse field " + pv->getFieldName() + " (float array at index " + soffset + ") from string value '" + fromString + "'");
            }
            data[0] = fval;
            if (pvdata->put(offset, 1, data, 0) == 0)
                return ntransfered;
            --len;
            ++ntransfered;
            ++offset;
            ++fromOffset;
        }
        return ntransfered;
    }
    case pvDouble: {
        PVDoubleArray *pvdata = static_cast<PVDoubleArray*>(pv);
        double data[1];
        while (len > 0) {
            String fromString = from[fromOffset];
            double fval;
            int result = sscanf(fromString.c_str(),"%lf",&fval);
            if (result != 1)
            {
            	char soffset[64];
            	sprintf(soffset, "%u", (unsigned int)offset);
            	throw runtime_error("failed to parse field " + pv->getFieldName() + " (double array at index " + soffset + ") from string value '" + fromString + "'");
            }
            data[0] = fval;
            if (pvdata->put(offset, 1, data, 0) == 0)
                return ntransfered;
            --len;
            ++ntransfered;
            ++offset;
            ++fromOffset;
        }
        return ntransfered;
    }
    case pvString:
        PVStringArray *pvdata = static_cast<PVStringArray*>(pv);
        while (len > 0) {
            String * xxx = const_cast<String *>(get(from));
            size_t n = pvdata->put(offset, len, xxx, fromOffset);
            if (n == 0)
                break;
            len -= n;
            offset += n;
            fromOffset += n;
            ntransfered += n;
        }
        return ntransfered;
    }
    String message("convertFromStringArray should never get here");
    throw std::logic_error(message);
}

size_t fromStringArray(PVScalarArrayPtr const &pv, size_t offset, size_t length,
    StringArray const & from, size_t fromOffset)
{
    return convertFromStringArray(pv.get(),offset,length,from,fromOffset);
}

size_t fromString(PVScalarArrayPtr const &pv, StringArray const & from, size_t fromStartIndex = 0)
{
	int processed = 0;
	size_t fromValueCount = from.size();

	// first get count
	if (fromStartIndex >= fromValueCount)
		throw std::runtime_error("not enough values, stopped at field " + pv->getFieldName());

	size_t count;
	istringstream iss(from[fromStartIndex]);
	iss >> count;
	// not fail and entire value is parsed (e.g. to detect 1.2 parsing to 1)
	if (iss.fail() || !iss.eof())
    	throw runtime_error("failed to parse element count value (uint) of field '" + pv->getFieldName() + "' from string value '" + from[fromStartIndex] + "'");
	fromStartIndex++;
	processed++;

	if ((fromStartIndex+count) > fromValueCount)
	{
    	throw runtime_error("not enough array values for field " + pv->getFieldName());
	}

    StringArray valueList;
    valueList.reserve(count);
    for(size_t i=0; i<count; i++)
    	valueList.push_back(from[fromStartIndex++]);
    processed += count;

    size_t num = fromStringArray(pv,0,count,valueList,0);
    pv->setLength(num);

    return processed;
}

size_t fromString(PVStructurePtr const & pvStructure, StringArray const & from, size_t fromStartIndex = 0)
{
    size_t processed = 0;
    size_t fromValueCount = from.size();

    PVFieldPtrArray const & fieldsData = pvStructure->getPVFields();
    if (fieldsData.size() != 0) {
        size_t length = pvStructure->getStructure()->getNumberFields();
        for(size_t i = 0; i < length; i++) {
            PVFieldPtr fieldField = fieldsData[i];

            Type type = fieldField->getField()->getType();
            if(type==structure) {
                PVStructurePtr pv = static_pointer_cast<PVStructure>(fieldField);
                size_t count = fromString(pv, from, fromStartIndex);
                processed += count;
                fromStartIndex += count;
            }
            else if(type==scalarArray) {
                PVScalarArrayPtr pv = static_pointer_cast<PVScalarArray>(fieldField);
                size_t count = fromString(pv, from, fromStartIndex);
                processed += count;
                fromStartIndex += count;
            }
            else if(type==scalar) {

            	if (fromStartIndex >= fromValueCount)
            		throw std::runtime_error("not enough values, stopped at field " + fieldField->getFieldName());

            	PVScalarPtr pv = static_pointer_cast<PVScalar>(fieldField);
                fromString(pv, from[fromStartIndex++]);
                processed++;
            }
            else {
                // structureArray not supported
                String message("fromString unsupported fieldType ");
                TypeFunc::toString(&message,type);
                throw std::logic_error(message);
            }
        }
    }

    return processed;
}



#define DEFAULT_TIMEOUT 3.0
#define DEFAULT_REQUEST "field(value)"

double timeOut = DEFAULT_TIMEOUT;
string request(DEFAULT_REQUEST);

enum PrintMode { ValueOnlyMode, StructureMode, TerseMode };
PrintMode mode = ValueOnlyMode;

char fieldSeparator = ' ';

void usage (void)
{
    fprintf (stderr, "\nUsage: pvput [options] <PV name> <values>...\n\n"
    "  -h: Help: Print this message\n"
    "options:\n"
    "  -r <pv request>:   Request, specifies what fields to return and options, default is '%s'\n"
    "  -w <sec>:          Wait time, specifies timeout, default is %f second(s)\n"
    "  -t:                Terse mode - print only successfully written value, without names\n"
    "  -q:                Quiet mode, print only error messages\n"
    "  -d:                Enable debug output\n"
    "  -F <ofs>:          Use <ofs> as an alternate output field separator"
    "\nExample: pvput double01 1.234\n\n"
             , DEFAULT_REQUEST, DEFAULT_TIMEOUT);
}

void printValue(String const & channelName, PVStructure::shared_pointer const & pv)
{
    if (mode == ValueOnlyMode)
    {
        PVField::shared_pointer value = pv->getSubField("value");
        if (value.get() == 0)
        {
        	std::cerr << "no 'value' field" << std::endl;
            std::cout << std::endl << *(pv.get()) << std::endl << std::endl;
        }
        else
        {
			Type valueType = value->getField()->getType();
			if (valueType != scalar && valueType != scalarArray)
			{
				// switch to structure mode
				std::cout << channelName << std::endl << *(pv.get()) << std::endl << std::endl;
			}
			else
			{
				if (fieldSeparator == ' ' && value->getField()->getType() == scalar)
					std::cout << std::setw(30) << std::left << channelName;
				else
					std::cout << channelName;

				std::cout << fieldSeparator;

				terse(std::cout, value) << std::endl;
			}
        }
    }
    else if (mode == TerseMode)
        terseStructure(std::cout, pv) << std::endl;
    else
        std::cout << std::endl << *(pv.get()) << std::endl << std::endl;
}

struct AtomicBoolean_null_deleter
{
    void operator()(void const *) const {}
};

// standard performance on set/clear, use of tr1::shared_ptr lock-free counter for get
// alternative is to use boost::atomic
class AtomicBoolean
{
    public:
        AtomicBoolean() : counter(static_cast<void*>(0), AtomicBoolean_null_deleter()) {};

        void set() { mutex.lock(); setp = counter; mutex.unlock(); }
        void clear() { mutex.lock(); setp.reset(); mutex.unlock(); }

        bool get() const { return counter.use_count() == 2; }
    private:
        std::tr1::shared_ptr<void> counter;
        std::tr1::shared_ptr<void> setp;
        epics::pvData::Mutex mutex;
};

class ChannelPutRequesterImpl : public ChannelPutRequester
{
    private:
    ChannelPut::shared_pointer m_channelPut;
    PVStructure::shared_pointer m_pvStructure;
    BitSet::shared_pointer m_bitSet;
    Mutex m_pointerMutex;
    Mutex m_eventMutex;
    auto_ptr<Event> m_event;
    String m_channelName;
    AtomicBoolean m_done;

    public:
    
    ChannelPutRequesterImpl(String channelName) : m_channelName(channelName)
    {
    	resetEvent();
    }
    
    virtual String getRequesterName()
    {
        return "ChannelPutRequesterImpl";
    }

    virtual void message(String const & message, MessageType messageType)
    {
        std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelPutConnect(const epics::pvData::Status& status,
    							   ChannelPut::shared_pointer const & channelPut,
                                   epics::pvData::PVStructure::shared_pointer const & pvStructure, 
                                   epics::pvData::BitSet::shared_pointer const & bitSet)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel put create: " << status << std::endl;
            }

            // assign smart pointers
            {
                Lock lock(m_pointerMutex);
                m_channelPut = channelPut;
                m_pvStructure = pvStructure;
            	m_bitSet = bitSet;
            }
            
            // we always put all
            m_bitSet->set(0);
            
            // get immediately old value
            channelPut->get();
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to create channel put: " << status << std::endl;
            m_event->signal();
        }
    }

    virtual void getDone(const epics::pvData::Status& status)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel get: " << status << std::endl;
            }

        	m_done.set();

        	/*
            // access smart pointers
            // do not print old value in terseMode
            if (!m_supressGetValue.get())
            {
                Lock lock(m_pointerMutex);
                {

                    // needed since we access the data
                    ScopedLock dataLock(m_channelPut);

                    printValue(m_channelName, m_pvStructure);
                }
            }
            */

        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to get: " << status << std::endl;
        }
        
        m_event->signal();
    }

    virtual void putDone(const epics::pvData::Status& status)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel put: " << status << std::endl;
            }
  
            m_done.set();
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to put: " << status << std::endl;
        }
        
        m_event->signal();
    }

    PVStructure::shared_pointer getStructure()
    {
        Lock lock(m_pointerMutex);
        return m_pvStructure;
    }

    void resetEvent()
    {
        Lock lock(m_eventMutex);
        m_event.reset(new Event());
        m_done.clear();
    }
    
    bool waitUntilDone(double timeOut)
    {
    	Event* event;
    	{
    		Lock lock(m_eventMutex);
    		event = m_event.get();
    	}

        bool signaled = event->wait(timeOut);
        if (!signaled)
        {
            std::cerr << "[" << m_channelName << "] timeout" << std::endl;
            return false;
    	}

        return m_done.get();
    }

};

/*+**************************************************************************
 *
 * Function:	main
 *
 * Description:	pvput main()
 * 		Evaluate command line options, set up PVA, connect the
 * 		channels, print the data as requested
 *
 * Arg(s) In:	[options] <pv-name> <values>...
 *
 * Arg(s) Out:	none
 *
 * Return(s):	Standard return code (0=success, 1=error)
 *
 **************************************************************************-*/

int main (int argc, char *argv[])
{
    int opt;                    /* getopt() current option */
    bool debug = false;
    bool quiet = false;

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */
    putenv(const_cast<char*>("POSIXLY_CORRECT="));            /* Behave correct on GNU getopt systems; e.g. handle negative numbers */

    while ((opt = getopt(argc, argv, ":hr:w:tqdF:")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'w':               /* Set PVA timeout value */
            if(epicsScanDouble(optarg, &timeOut) != 1)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('pvput -h' for help.)\n", optarg);
                timeOut = DEFAULT_TIMEOUT;
            }
            break;
        case 'r':               /* Set PVA timeout value */
            request = optarg;
            // do not override terse mode
            if (mode == ValueOnlyMode) mode = StructureMode;
            break;
        case 't':               /* Terse mode */
            mode = TerseMode;
            break;
        case 'd':               /* Debug log level */
            debug = true;
            break;
        case 'q':               /* Quiet mode */
            quiet = true;
            break;
        case 'F':               /* Store this for output formatting */
            fieldSeparator = (char) *optarg;
            break;
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('pvput -h' for help.)\n",
                    optopt);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('pvput -h' for help.)\n",
                    optopt);
            return 1;
        default :
            usage();
            return 1;
        }
    }

    if (argc <= optind)
    {
        fprintf(stderr, "No pv name specified. ('pvput -h' for help.)\n");
        return 1;
    }
    string pvName = argv[optind++];
    

    int nVals = argc - optind;       /* Remaining arg list are PV names */
    if (nVals < 1)
    {
        fprintf(stderr, "No value(s) specified. ('pvput -h' for help.)\n");
        return 1;
    }

    vector<string> values;     /* Array of values */
    for (int n = 0; optind < argc; n++, optind++)
        values.push_back(argv[optind]);       /* Copy values from command line */

    Requester::shared_pointer requester(new RequesterImpl("pvput"));

    PVStructure::shared_pointer pvRequest = getCreateRequest()->createRequest(request, requester);
    if(pvRequest.get()==NULL) {
        fprintf(stderr, "failed to parse request string\n");
        return 1;
    }
    
    SET_LOG_LEVEL(debug ? logLevelDebug : logLevelError);

    std::cout << std::boolalpha;
    terseSeparator(fieldSeparator);

    ClientFactory::start();
    ChannelProvider::shared_pointer provider = getChannelAccess()->getProvider("pvAccess");

    //epics::pvAccess::ca::CAClientFactory::start();
    //ChannelProvider::shared_pointer provider = getChannelAccess()->getProvider("ca");

    bool allOK = true;

    try
    {
        do
        {
            // first connect
            shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl(quiet));
            Channel::shared_pointer channel = provider->createChannel(pvName, channelRequesterImpl);

            if (channelRequesterImpl->waitUntilConnected(timeOut))
            {
                shared_ptr<ChannelPutRequesterImpl> putRequesterImpl(new ChannelPutRequesterImpl(channel->getChannelName()));
                if (mode != TerseMode && !quiet)
                	std::cout << "Old : ";
                ChannelPut::shared_pointer channelPut = channel->createChannelPut(putRequesterImpl, pvRequest);
                allOK &= putRequesterImpl->waitUntilDone(timeOut);
                if (allOK)
                {
                    if (mode != TerseMode && !quiet)
                        printValue(pvName, putRequesterImpl->getStructure());

                	// convert value from string
                	// since we access structure from another thread, we need to lock
                	{
						ScopedLock lock(channelPut);
						fromString(putRequesterImpl->getStructure(), values);
                	}

                    // we do a put
                    putRequesterImpl->resetEvent();
                    channelPut->put(false);
                    allOK &= putRequesterImpl->waitUntilDone(timeOut);
        
                    if (allOK)
                    {
                        // and than a get again to verify put
                        if (mode != TerseMode && !quiet) std::cout << "New : ";
                        putRequesterImpl->resetEvent();
                        channelPut->get();
                        allOK &= putRequesterImpl->waitUntilDone(timeOut);
                        if (allOK && !quiet)
                            printValue(pvName, putRequesterImpl->getStructure());
                    }
                }
            }
            else
            {
                allOK = false;
                channel->destroy();
                std::cerr << "[" << channel->getChannelName() << "] connection timeout" << std::endl;
                break;
            }
        }
        while (false);
    } catch (std::out_of_range& oor) {
        allOK = false;
        std::cerr << "parse error: not enough of values" << std::endl;
    } catch (std::exception& ex) {
        allOK = false;
        std::cerr << ex.what() << std::endl;
    } catch (...) {
        allOK = false;
        std::cerr << "unknown exception caught" << std::endl;
    }
        
    ClientFactory::stop();

    return allOK ? 0 : 1;
}
