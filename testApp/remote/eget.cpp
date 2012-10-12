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
#include <ostream>
#include <sstream>
#include <iomanip>
#include <map>

#include <pv/event.h>
#include <epicsExit.h>

#include "pvutils.cpp"

using namespace std;
using namespace std::tr1;
using namespace epics::pvData;
using namespace epics::pvAccess;

enum PrintMode { ValueOnlyMode, StructureMode, TerseMode };
PrintMode mode = ValueOnlyMode;

char fieldSeparator = ' ';


void formatNTAny(std::ostream& o, PVStructurePtr const & pvStruct)
{
    PVFieldPtr value = pvStruct->getSubField("value");
    if (value.get() == 0)
    {
    	std::cerr << "no 'value' column in NTAny" << std::endl;
        return;
    }

    o << *value;
}

void formatNTScalar(std::ostream& o, PVStructurePtr const & pvStruct)
{
    PVScalarPtr value = dynamic_pointer_cast<PVScalar>(pvStruct->getSubField("value"));
    if (value.get() == 0)
    {
    	std::cerr << "no scalar_t 'value' column in NTScalar" << std::endl;
        return;
    }

    o << *value;
}

std::ostream& formatVector(std::ostream& o,
		String label,
		PVScalarArrayPtr const & pvScalarArray,
		bool transpose)
{
	size_t len = pvScalarArray->getLength();

	if (!transpose)
	{
		if (!label.empty())
			o << label << std::endl;

		for (size_t i = 0; i < len; i++)
			pvScalarArray->dumpValue(o, i) << std::endl;
	}
	else
	{
		bool first = true;
		if (!label.empty())
		{
			o << label;
			first = false;
		}

	    for (size_t i = 0; i < len; i++) {
			if (first)
				first = false;
			else
				o << fieldSeparator;

			pvScalarArray->dumpValue(o, i);
	    }
	}

	return o;
}

/*
std::ostream& formatScalarArray(std::ostream& o, PVScalarArrayPtr const & pvScalarArray)
{
	size_t len = pvScalarArray->getLength();
	if (len == 0)
	{
		// TODO do we really want this
		o << "(empty)" << std::endl;
	}
	else
	{
		for (size_t i = 0; i < len; i++)
			pvScalarArray->dumpValue(o, i) << std::endl;
	}
	return o;
}
*/

void formatNTScalarArray(std::ostream& o, PVStructurePtr const & pvStruct)
{
    PVScalarArrayPtr value = dynamic_pointer_cast<PVScalarArray>(pvStruct->getSubField("value"));
    if (value.get() == 0)
    {
    	std::cerr << "no scalar_t[] 'value' column in NTScalarArray" << std::endl;
        return;
    }

    //o << *value;
    //formatScalarArray(o, value);
    formatVector(o, "", value, mode == TerseMode);
}

size_t getLongestString(PVScalarArrayPtr const & array)
{
	size_t max = 0;

	string empty;

	ostringstream oss;
	size_t len = array->getLength();
    for (size_t i = 0; i < len; i++)
    {
    	oss.str(empty);
       	array->dumpValue(oss, i);
       	size_t l = oss.tellp();
       	if (l > max) max = l;
    }
    return max;
}

// labels are optional
// if provided labels.size() must equals columnData.size()
void formatTable(std::ostream& o,
		PVStringArrayPtr const & labels,
		vector<PVScalarArrayPtr> const & columnData,
		bool transpose)
{
	// array with maximum number of elements
    size_t maxValues = 0;

    // value with longest string form
    size_t maxColumnLength = labels.get() ? getLongestString(labels) : 0;

    //
    // get maxValue and maxColumnLength
    //
    size_t numColumns = columnData.size();
    for (size_t i = 0; i < numColumns; i++)
    {
    	PVScalarArrayPtr array = columnData[i];

    	size_t arrayLength = array->getLength();
    	if (maxValues < arrayLength) maxValues = arrayLength;

        size_t colLen = getLongestString(array);
        if (colLen > maxColumnLength) maxColumnLength = colLen;
    }

    // add some space
    size_t padding = 2;
    maxColumnLength += padding;

    // get labels
   	StringArrayData labelsData;
    labels->get(0, numColumns, labelsData);

    if (!transpose)
    {

		//
		// <column0>, <column1>, ...
		//   values     values   ...
		//

		// first print labels
		for (size_t i = 0; i < numColumns; i++)
		{
			o << std::setw(maxColumnLength) << std::right << labelsData.data[i];
		}
		o << std::endl;

		// then values
		for (size_t r = 0; r < maxValues; r++)
		{
			for (size_t i = 0; i < numColumns; i++)
			{
				o << std::setw(maxColumnLength) << std::right;
				if (r < columnData[i]->getLength())
					columnData[i]->dumpValue(o, r);
				else
					o << "";
			}
			o << std::endl;
		}

    }
    else
    {

		//
		// <column0> values...
		// <column1> values...
		// ...
		//

		for (size_t i = 0; i < numColumns; i++)
		{
			o << std::setw(maxColumnLength) << std::left << labelsData.data[i];
			for (size_t r = 0; r < maxValues; r++)
			{
				o << std::setw(maxColumnLength) << std::right;
				if (r < columnData[i]->getLength())
					columnData[i]->dumpValue(o, r);
				else
					o << "";
			}
			o << std::endl;
		}

    }
}

void formatNTTable(std::ostream& o, PVStructurePtr const & pvStruct)
{
    PVStringArrayPtr labels = dynamic_pointer_cast<PVStringArray>(pvStruct->getScalarArrayField("labels", pvString));
    if (labels.get() == 0)
    {
    	std::cerr << "no string[] 'labels' column in NTTable" << std::endl;
        return;
    }

    PVStructurePtr value = pvStruct->getStructureField("value");
    if (value.get() == 0)
    {
    	std::cerr << "no 'value' structure in NTTable" << std::endl;
        return;
    }

    vector<PVScalarArrayPtr> columnData;
    PVFieldPtrArray fields = value->getPVFields();
    size_t numColumns = fields.size();

    if (labels->getLength() != numColumns)
    {
       	std::cerr << "malformed NTTable, length of 'labels' array does not equal to a number of 'value' structure subfields" << std::endl;
       	return;
    }

    for (size_t i = 0; i < numColumns; i++)
    {
    	PVScalarArrayPtr array = dynamic_pointer_cast<PVScalarArray>(fields[i]);
    	if (array.get() == 0)
    	{
        	std::cerr << "malformed NTTable, " << (i+1) << ". field is not scalar_t[]" << std::endl;
    		return;
    	}

        columnData.push_back(array);
    }

    formatTable(o, labels, columnData, mode == TerseMode);
}    


void formatNTMatrix(std::ostream& o, PVStructurePtr const & pvStruct)
{
    PVDoubleArrayPtr value = dynamic_pointer_cast<PVDoubleArray>(pvStruct->getScalarArrayField("value", pvDouble));
    if (value.get() == 0)
    {
    	std::cerr << "no double[] 'value' column in NTMatrix" << std::endl;
        return;
    }

    int32 rows, cols;

    PVIntArrayPtr dim = dynamic_pointer_cast<PVIntArray>(pvStruct->getScalarArrayField("dim", pvInt));
    if (dim.get() != 0)
    {
		// dim[] = { rows, columns }
		size_t dims = dim->getLength();
		if (dims != 1 && dims != 2)
		{
			std::cerr << "malformed NTMatrix, dim[] must contain 1 or 2 elements instead of  " << dims << std::endl;
			return;
		}

	    IntArrayData data;
	    dim->get(0, dims, data);
	    rows = data.data[0];
	    cols = (dims == 2) ? data.data[1] : 1;

		if (rows <= 0 || cols <= 0)
		{
			std::cerr << "malformed NTMatrix, dim[] must contain elements >= 0" << std::endl;
			return;
		}
    }
    else
    {
    	rows = value->getLength();
    	cols = 1;
    }

    o << std::left;

    size_t len = static_cast<size_t>(rows*cols);
    if (len != value->getLength())
    {
		std::cerr << "malformed NTMatrix, values[] must contain " << len << " elements instead of  " << value->getLength() << std::endl;
		return;
    }

    // add some space
    size_t padding = 2;
    size_t maxColumnLength = getLongestString(value) + padding;

    // TerseMode as Transpose
    if (mode != TerseMode)
    {

		//
		// el1 el2 el3
		// el4 el5 el6
		//

		size_t ix = 0;
		for (int32 r = 0; r < rows; r++)
		{
			for (int32 c = 0; c < cols; c++)
			{
				o << std::setw(maxColumnLength) << std::right;
				value->dumpValue(o, ix++);
			}
			o << std::endl;
		}

    }
    else
    {
		//
		// el1 el4
		// el2 el5
		// el3 el6
		//
		for (int32 c = 0; c < cols; c++)
		{
			for (int32 r = 0; r < rows; r++)
			{
				o << std::setw(maxColumnLength) << std::right;
				value->dumpValue(o, r * rows + c);
			}
			o << std::endl;
		}
    }
}

typedef void(*NTFormatterFunc)(std::ostream& o, PVStructurePtr const & pvStruct);
typedef map<String, NTFormatterFunc> NTFormatterLUTMap;
NTFormatterLUTMap ntFormatterLUT;

void initializeNTFormatterLUT()
{
	ntFormatterLUT["uri:ev4:nt/2012/pwd:NTScalar"] = formatNTScalar;
	ntFormatterLUT["uri:ev4:nt/2012/pwd:NTScalarArray"] = formatNTScalarArray;
	ntFormatterLUT["uri:ev4:nt/2012/pwd:NTTable"] = formatNTTable;
	ntFormatterLUT["uri:ev4:nt/2012/pwd:NTMatrix"] = formatNTMatrix;
	ntFormatterLUT["uri:ev4:nt/2012/pwd:NTAny"] = formatNTAny;

	//
	// TODO remove: smooth transition
	//

	ntFormatterLUT["NTScalar"] = formatNTScalar;
	ntFormatterLUT["NTScalarArray"] = formatNTScalarArray;
	ntFormatterLUT["NTTable"] = formatNTTable;
	ntFormatterLUT["NTMatrix"] = formatNTMatrix;
	ntFormatterLUT["NTAny"] = formatNTAny;

	// StandardPV "support"
	ntFormatterLUT["scalar_t"] = formatNTScalar;
	ntFormatterLUT["scalarArray_t"] = formatNTScalarArray;
}

void formatNT(std::ostream& o, PVFieldPtr const & pv)
{
	static bool lutInitialized = false;
	if (!lutInitialized)
	{
		initializeNTFormatterLUT();
		lutInitialized = true;
	}

    Type type = pv->getField()->getType();
    if (type==structure)
    {
        PVStructurePtr pvStruct = static_pointer_cast<PVStructure>(pv);
        {
            String id = pvStruct->getField()->getID();

            NTFormatterLUTMap::const_iterator formatter = ntFormatterLUT.find(id);
            if (formatter != ntFormatterLUT.end())
            {
            	(formatter->second)(o, pvStruct);
            }
            else
            {
                std::cerr << "unsupported normative type" << std::endl;
                o << *(pv.get()) << std::endl;
            }

            return;
        }
    }
    
    // no ID, just dump
    o << *(pv.get()) << std::endl;
}








#define DEFAULT_TIMEOUT 3.0
#define DEFAULT_REQUEST "field(value)"

double timeOut = DEFAULT_TIMEOUT;
string request(DEFAULT_REQUEST);


void usage (void)
{
    fprintf (stderr, "\nUsage: eget [options] [<PV name>... | -s <service name>]\n\n"
    "  -h: Help: Print this message\n"
    "\noptions:\n"
    "  -s <service name>:   Service API compliant based RPC service name (accepts NTURI request argument)\n"
    "  -a <service arg>:    Service argument in form 'name=value'\n"
    "  -r <pv request>:     Get request string, specifies what fields to return and options, default is '%s'\n"
    "  -w <sec>:            Wait time, specifies timeout, default is %f second(s)\n"
    "  -q:					Pure pvAccess RPC based service (send NTURI.query as request argument)\n"
    "  -t:                  Terse mode - print only value, without field names\n"
    "  -d:                  Enable debug output\n"
    "  -F <ofs>:          Use <ofs> as an alternate output field separator\n"
    "  -c:                  Wait for clean shutdown and report used instance count (for expert users)"
    "\n\nexamples:\n\n"
"#! Get the value of the PV corr:li32:53:bdes\n"
"> eget corr:li32:53:bdes\n"
"\n"
"#! Get the table of all correctors from the rdb service\n"
"> eget -s rdbService -a entity=swissfel:devicenames\n"
"\n"
"#! Get the archive history of quad45:bdes;history between 2 times, from the archive service\n"
"> eget -s archiveService -a entity=quad45:bdes;history -a starttime=2012-02-12T10:04:56 -a endtime=2012-02-01T10:04:56\n"
"\n"
             , DEFAULT_REQUEST, DEFAULT_TIMEOUT);
}



class ChannelGetRequesterImpl : public ChannelGetRequester
{
    private:
    ChannelGet::shared_pointer m_channelGet;
    PVStructure::shared_pointer m_pvStructure;
    BitSet::shared_pointer m_bitSet;
    Mutex m_pointerMutex;
    Event m_event;
    String m_channelName;

    public:
    
    ChannelGetRequesterImpl(String channelName) : m_channelName(channelName) {}
    
    virtual String getRequesterName()
    {
        return "ChannelGetRequesterImpl";
    }

    virtual void message(String const & message, MessageType messageType)
    {
        std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelGetConnect(const epics::pvData::Status& status,ChannelGet::shared_pointer const & channelGet,
                                   epics::pvData::PVStructure::shared_pointer const & pvStructure, 
                                   epics::pvData::BitSet::shared_pointer const & bitSet)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel get create: " << status.toString() << std::endl;
            }
            
            // assign smart pointers
            {
                Lock lock(m_pointerMutex);
                m_channelGet = channelGet;
                m_pvStructure = pvStructure;
                m_bitSet = bitSet;
            }
            
            channelGet->get(true);
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to create channel get: " << status.toString() << std::endl;
        }
    }

    virtual void getDone(const epics::pvData::Status& status)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel get: " << status.toString() << std::endl;
            }

            // access smart pointers
            {
                Lock lock(m_pointerMutex);
                {
                    // needed since we access the data
                    ScopedLock dataLock(m_channelGet);
    
                    if (mode == ValueOnlyMode)
                    {
                        PVField::shared_pointer value = m_pvStructure->getSubField("value");
                        if (value.get() == 0)
                        {
                        	std::cerr << "no 'value' field" << std::endl;
                            std::cout << m_channelName << std::endl << *(m_pvStructure.get()) << std::endl << std::endl;
                        }
                        else
                        {
							Type valueType = value->getField()->getType();
							if (valueType == scalar)
								std::cout << *(value.get()) << std::endl;
							else if (valueType == scalarArray)
							{
								//formatScalarArray(std::cout, dynamic_pointer_cast<PVScalarArray>(value));
								formatVector(std::cout, "", dynamic_pointer_cast<PVScalarArray>(value), false);
							}
							else
							{
								// switch to structure mode
								std::cout << m_channelName << std::endl << *(m_pvStructure.get()) << std::endl << std::endl;
							}
                        }
                    }
                    else if (mode == TerseMode)
                        terseStructure(std::cout, m_pvStructure) << std::endl;
                    else
                        std::cout << m_channelName << std::endl << *(m_pvStructure.get()) << std::endl << std::endl;
                }
                // this is OK since calle holds also owns it
                m_channelGet.reset();
            }

            m_event.signal();

        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to get: " << status.toString() << std::endl;
            {
                Lock lock(m_pointerMutex);
                // this is OK since caller holds also owns it
                m_channelGet.reset();
            }
        }

    }

    bool waitUntilGet(double timeOut)
    {
        return m_event.wait(timeOut);
    }
};

/*
PVFieldPtr pvField = m_pvStructure->getSubField("value");
                    	if (pvField.get())
                    	{
                    		PVScalarArrayPtr pvScalarArray = std::tr1::dynamic_pointer_cast<PVScalarArray>(pvField);
                    		if (pvScalarArray.get())
                    		{
                    			size_t len = pvScalarArray->getLength();
                    			for (size_t i = 0; i < len; i++)
                    			{
                        			pvScalarArray->dumpValue(std::cout, i) << std::endl;
                    			}
                    		}
                    		else
                    		{
                    			std::cout << *(pvField.get()) << std::endl;
                    		}
                    	}
                    	else
                    	{
                    		// do a structure mode, as fallback
                    		std::cerr << "no 'value' field" << std::endl;
                            String str;
                            m_pvStructure->toString(&str);
                            std::cout << str << std::endl;
                    	}
                    }
                    else if (mode == TerseMode)
                    {
                        String str;
                        convertToString(&str, m_pvStructure.get(), 0);
                        std::cout << str << std::endl;
                    }
                    else //if (mode == StructureMode)
                    {
                        String str;
                        m_pvStructure->toString(&str);
                        std::cout << str << std::endl;
                    }
                } 
*/

class ChannelRPCRequesterImpl : public ChannelRPCRequester
{
    private:
    ChannelRPC::shared_pointer m_channelRPC;
    Mutex m_pointerMutex;
    Event m_event;
    Event m_connectionEvent;
    String m_channelName;

    public:
    
    ChannelRPCRequesterImpl(String channelName) : m_channelName(channelName) {}
    
    virtual String getRequesterName()
    {
        return "ChannelRPCRequesterImpl";
    }

    virtual void message(String const & message, MessageType messageType)
    {
        std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelRPCConnect(const epics::pvData::Status& status,ChannelRPC::shared_pointer const & channelRPC)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel RPC create: " << status.toString() << std::endl;
            }
            
            // assign smart pointers
            {
                Lock lock(m_pointerMutex);
                m_channelRPC = channelRPC;
            }
            
            m_connectionEvent.signal();
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to create channel get: " << status.toString() << std::endl;
        }
    }

    virtual void requestDone (const epics::pvData::Status &status, epics::pvData::PVStructure::shared_pointer const &pvResponse)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel RPC: " << status.toString() << std::endl;
            }

            // access smart pointers
            {
                Lock lock(m_pointerMutex);

                formatNT(std::cout, pvResponse);
                std::cout << std::endl;

                // this is OK since calle holds also owns it
                m_channelRPC.reset();
            }
            
            m_event.signal();
            
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to RPC: " << status.toString() << std::endl;
            {
                Lock lock(m_pointerMutex);
                // this is OK since caller holds also owns it
                m_channelRPC.reset();
            }
        }
        
    }
    
    /*
    void request(epics::pvData::PVStructure::shared_pointer const &pvRequest)
    {
        Lock lock(m_pointerMutex);
        m_channelRPC->request(pvRequest, false);
    }
    */

    bool waitUntilRPC(double timeOut)
    {
        return m_event.wait(timeOut);
    }

    bool waitUntilConnected(double timeOut)
    {
        return m_connectionEvent.wait(timeOut);
    }
};

/*+**************************************************************************
 *
 * Function:	main
 *
 * Description:	eget main()
 * 		Evaluate command line options, set up CA, connect the
 * 		channels, print the data as requested
 *
 * Arg(s) In:	[options] <pv-name>...
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
    bool cleanupAndReport = false;

    bool serviceRequest = false;
    bool onlyQuery = false;
    string service;
    string urlEncodedRequest;
    vector< pair<string,string> > parameters;

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */

    while ((opt = getopt(argc, argv, ":hr:s:a:w:qtdcF:")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'w':               /* Set CA timeout value */
            if(epicsScanDouble(optarg, &timeOut) != 1)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('eget -h' for help.)\n", optarg);
                timeOut = DEFAULT_TIMEOUT;
            }
            break;
        case 'r':               /* Set pvRequest value */
            request = optarg;
            // do not override terse mode
            if (mode == ValueOnlyMode) mode = StructureMode;
            break;          
        case 'a':               /* Service parameters */
        {   
            string param = optarg;
            size_t eqPos = param.find('=');
            if (eqPos==string::npos)
            {
                fprintf(stderr, "Parameter not specified in name=value form. ('eget -h' for help.)\n");
                return 1;
            }
            parameters.push_back(pair<string,string>(param.substr(0, eqPos), param.substr(eqPos+1, string::npos)));    
            if (urlEncodedRequest.size())
                urlEncodedRequest += '&';    
            char* encoded = url_encode(optarg);
            urlEncodedRequest += encoded;
            free(encoded);
            break;
        }
        case 's':               /* Service name */
            service = optarg;
            serviceRequest = true;
            break;
        case 'q':               /* pvAccess RPC mode */
            onlyQuery = true;
            break;
        case 't':               /* Terse mode */
            mode = TerseMode;
            break;
        case 'd':               /* Debug log level */
            debug = true;
            break;
        case 'c':               /* Clean-up and report used instance count */
            cleanupAndReport = true;
            break;
        case 'F':               /* Store this for output formatting */
            fieldSeparator = (char) *optarg;
            break;
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('eget -h' for help.)\n",
                    optopt);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('eget -h' for help.)\n",
                    optopt);
            return 1;
        default :
            usage();
            return 1;
        }
    }

    int nPvs = argc - optind;       /* Remaining arg list are PV names */
    if (nPvs < 1 && !serviceRequest)
    {
        fprintf(stderr, "No PV name(s) specified. ('eget -h' for help.)\n");
        return 1;
    }
    
    if (nPvs > 0 && serviceRequest)
    {
        fprintf(stderr, "PV name(s) specified and service query requested. ('eget -h' for help.)\n");
        return 1;
    }

    SET_LOG_LEVEL(debug ? logLevelDebug : logLevelError);

    std::cout << std::boolalpha;
    terseSeparator(fieldSeparator);
    terseArrayCount(false);

    bool allOK = true;

    Requester::shared_pointer requester(new RequesterImpl("eget"));

    // PVs mode
    if (!serviceRequest)
    {
        vector<string> pvs;     /* Array of PV structures */
        for (int n = 0; optind < argc; n++, optind++)
            pvs.push_back(argv[optind]);       /* Copy PV names from command line */
        
        PVStructure::shared_pointer pvRequest =
        		getCreateRequest()->createRequest(request, requester);
        if(pvRequest.get()==0) {
        	fprintf(stderr, "failed to parse request string\n");
            return 1;
        }
        
        ClientFactory::start();
        ChannelProvider::shared_pointer provider = getChannelAccess()->getProvider("pvAccess");
    
        // first connect to all, this allows resource (e.g. TCP connection) sharing
        vector<Channel::shared_pointer> channels(nPvs);
        for (int n = 0; n < nPvs; n++)
        {
            shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl()); 
            channels[n] = provider->createChannel(pvs[n], channelRequesterImpl);
        }
        
        // for now a simple iterating sync implementation, guarantees order
        for (int n = 0; n < nPvs; n++)
        {
            /*
            shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl()); 
            Channel::shared_pointer channel = provider->createChannel(pvs[n], channelRequesterImpl);
            */
            
            Channel::shared_pointer channel = channels[n];
            shared_ptr<ChannelRequesterImpl> channelRequesterImpl = dynamic_pointer_cast<ChannelRequesterImpl>(channel->getChannelRequester());
                
            if (channelRequesterImpl->waitUntilConnected(timeOut))
            {
            	shared_ptr<GetFieldRequesterImpl> getFieldRequesterImpl;

            	// probe for value field
            	if (mode == ValueOnlyMode)
            	{
            		getFieldRequesterImpl.reset(new GetFieldRequesterImpl(channel));
            		// get all to be immune to bad clients not supporting selective getField request
            		channel->getField(getFieldRequesterImpl, "");
            	}

            	if (getFieldRequesterImpl.get() == 0 ||
            		getFieldRequesterImpl->waitUntilFieldGet(timeOut))
            	{
            		// check probe
            		if (getFieldRequesterImpl.get())
            		{
						Structure::const_shared_pointer structure =
								dynamic_pointer_cast<const Structure>(getFieldRequesterImpl->getField());
						if (structure.get() == 0 || structure->getField("value").get() == 0)
						{
							// fallback to structure
							mode = StructureMode;
							pvRequest = getCreateRequest()->createRequest("field()", requester);
						}
            		}

            		shared_ptr<ChannelGetRequesterImpl> getRequesterImpl(new ChannelGetRequesterImpl(channel->getChannelName()));
                    ChannelGet::shared_pointer channelGet = channel->createChannelGet(getRequesterImpl, pvRequest);
                    allOK &= getRequesterImpl->waitUntilGet(timeOut);
            	}
            	else
            	{
            		allOK = false;
                    channel->destroy();
                    std::cerr << "[" << channel->getChannelName() << "] failed to get channel introspection data" << std::endl;
            	}
            }
            else
            {
                allOK = false;
                channel->destroy();
                std::cerr << "[" << channel->getChannelName() << "] connection timeout" << std::endl;
            }
        }    
    
        ClientFactory::stop();
    }
    // service RPC mode
    else
    {
    	/*
        std::cerr << "service            : " << service << std::endl;
        std::cerr << "parameters         : " << std::endl;

        vector< pair<string, string> >::iterator iter = parameters.begin();
        for (; iter != parameters.end(); iter++)
            std::cerr << "    " << iter->first << " = " << iter->second << std::endl;
        //std::cerr << "encoded URL request: '" << urlEncodedRequest << "'" << std::endl;
        */

        // simply empty
        PVStructure::shared_pointer pvRequest =
        		getCreateRequest()->createRequest(request, requester);
        if(pvRequest.get()==NULL) {
        	fprintf(stderr, "failed to parse request string\n");
            return 1;
        }
        

        StringArray queryFieldNames;
        FieldConstPtrArray queryFields;
        for (vector< pair<string, string> >::iterator iter = parameters.begin();
             iter != parameters.end();
             iter++)
        {
        	queryFieldNames.push_back(iter->first);
        	queryFields.push_back(getFieldCreate()->createScalar(pvString));
        }

        Structure::const_shared_pointer queryStructure(
        		getFieldCreate()->createStructure(
        				queryFieldNames,
        				queryFields
        			)
        	);



        StringArray uriFieldNames;
        uriFieldNames.push_back("path");
        uriFieldNames.push_back("query");

        FieldConstPtrArray uriFields;
        uriFields.push_back(getFieldCreate()->createScalar(pvString));
        uriFields.push_back(queryStructure);

        Structure::const_shared_pointer uriStructure(
        		getFieldCreate()->createStructure(
        				"uri:ev4:nt/2012/pwd:NTURI",
        				uriFieldNames,
        				uriFields
        			)
        	);



        PVStructure::shared_pointer request(
        		getPVDataCreate()->createPVStructure(uriStructure)
        	);

        request->getStringField("path")->put(service);
        PVStructure::shared_pointer query = request->getStructureField("query");
        for (vector< pair<string, string> >::iterator iter = parameters.begin();
             iter != parameters.end();
             iter++)
        {
        	query->getStringField(iter->first)->put(iter->second);
        }


        PVStructure::shared_pointer arg = onlyQuery ? query : request;
        if (debug)
        {
        	std::cout << "Request structure: " << std::endl << *(arg.get()) << std::endl;
        }


        ClientFactory::start();
        ChannelProvider::shared_pointer provider = getChannelAccess()->getProvider("pvAccess");
        
        shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl()); 
        Channel::shared_pointer channel = provider->createChannel(service, channelRequesterImpl);
        
        if (channelRequesterImpl->waitUntilConnected(timeOut))
        {
            shared_ptr<ChannelRPCRequesterImpl> rpcRequesterImpl(new ChannelRPCRequesterImpl(channel->getChannelName()));
            ChannelRPC::shared_pointer channelRPC = channel->createChannelRPC(rpcRequesterImpl, pvRequest);

            if (rpcRequesterImpl->waitUntilConnected(timeOut))
            {
				channelRPC->request(arg, true);
				allOK &= rpcRequesterImpl->waitUntilRPC(timeOut);
			}
            else
            {
                allOK = false;
                channel->destroy();
                std::cerr << "[" << channel->getChannelName() << "] RPC create timeout" << std::endl;
            }
        }
        else
        {
            allOK = false;
            channel->destroy();
            std::cerr << "[" << channel->getChannelName() << "] connection timeout" << std::endl;
        }
    
        ClientFactory::stop();
    }

    if (cleanupAndReport)
    {
        // TODO implement wait on context
        epicsThreadSleep ( 3.0 );
        //std::cerr << "-----------------------------------------------------------------------" << std::endl;
        //epicsExitCallAtExits();
    }

    return allOK ? 0 : 1;
}
