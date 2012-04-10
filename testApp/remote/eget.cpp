#include <iostream>
#include <pv/clientFactory.h>

#include <stdio.h>
#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <pv/logger.h>

#include <vector>
#include <string>


#include <pv/CDRMonitor.h>
#include <pv/event.h>
#include <epicsExit.h>

using namespace std;
using namespace std::tr1;
using namespace epics::pvData;
using namespace epics::pvAccess;



/// terse mode functions

void convertStructure(StringBuilder buffer,PVStructure *data,int notFirst);
void convertArray(StringBuilder buffer,PVScalarArray * pv,int notFirst);
void convertStructureArray(StringBuilder buffer,PVStructureArray * pvdata,int notFirst);








void convertToString(StringBuilder buffer,PVField * pv,int notFirst)
{
    Type type = pv->getField()->getType();
    if(type==structure) {
        convertStructure(buffer,static_cast<PVStructure*>(pv),notFirst);
        return;
    }
    if(type==scalarArray) {
        convertArray(buffer,static_cast<PVScalarArray *>(pv),notFirst);
        *buffer += "\t";
        return;
    }
    if(type==structureArray) {
    	convertStructureArray(
            buffer,static_cast<PVStructureArray*>(pv),notFirst);
        *buffer += "\t";
        return;
    }
    PVScalar *pvScalar = static_cast<PVScalar*>(pv);
    ScalarConstPtr pscalar = pvScalar->getScalar();
    ScalarType scalarType = pscalar->getScalarType();
    switch(scalarType) {
    case pvBoolean: {
            PVBoolean *data = static_cast<PVBoolean*>(pv);
            bool value = data->get();
            if(value) {
                *buffer += "true";
            } else {
                *buffer += "false";
            }
        }
        break;
    case pvByte: {
            PVByte *data = static_cast<PVByte*>(pv);
            char xxx[30];
            sprintf(xxx,"%d",(int)data->get());
            *buffer += xxx;
        }
        break;
    case pvShort: {
            PVShort *data = static_cast<PVShort*>(pv);
            char xxx[30];
            sprintf(xxx,"%d",(int)data->get());
            *buffer += xxx;
        }
        break;
    case pvInt: {
            PVInt *data = static_cast<PVInt*>(pv);
            char xxx[30];
            sprintf(xxx,"%d",(int)data->get());
            *buffer += xxx;
        }
        break;
    case pvLong: {
            PVLong *data = static_cast<PVLong*>(pv);
            char xxx[30];
            sprintf(xxx,"%lld",(int64)data->get());
            *buffer += xxx;
        }
        break;
    case pvFloat: {
            PVFloat *data = static_cast<PVFloat*>(pv);
            char xxx[30];
            sprintf(xxx,"%g",data->get());
            *buffer += xxx;
        }
        break;
    case pvDouble: {
            PVDouble *data = static_cast<PVDouble*>(pv);
            char xxx[30];
            sprintf(xxx,"%lg",data->get());
            *buffer += xxx;
        }
        break;
    case pvString: {
            PVString *data = static_cast<PVString*>(pv);
            *buffer += data->get();
        }
        break;
    default:
        *buffer += "(unknown ScalarType)";
    }
    
    *buffer += "\t";
}

void convertStructure(StringBuilder buffer,PVStructure *data,int notFirst)
{
    PVFieldPtrArray fieldsData = data->getPVFields();
    if (fieldsData != 0) {
        int length = data->getStructure()->getNumberFields();
        for(int i=0; i<length; i++) {
            PVField *fieldField = fieldsData[i];
            convertToString(buffer,fieldField,notFirst + 1);
        }
    }
}

void convertArray(StringBuilder buffer,PVScalarArray * pv,int notFirst)
{
    ScalarArrayConstPtr array = pv->getScalarArray();
    ScalarType type = array->getElementType();
    switch(type) {
    case pvBoolean: {
            PVBooleanArray *pvdata = static_cast<PVBooleanArray*>(pv);
            BooleanArrayData data = BooleanArrayData();
            *buffer += "[";
            for(int i=0; i < pvdata->getLength(); i++) {
                if(i!=0) *buffer += ",";
                int num = pvdata->get(i,1,&data);
                if(num==1) {
                     BooleanArray  value = data.data;
                     if(value[data.offset]) {
                         *buffer += "true";
                     } else {
                         *buffer += "false";
                     }
                } else {
                     *buffer += "???? ";
                }
            }
            *buffer += "]";
            break;
        }
    case pvByte: {
            PVByteArray *pvdata = static_cast<PVByteArray*>(pv);
            ByteArrayData data = ByteArrayData();
            *buffer += "[";
            for(int i=0; i < pvdata->getLength(); i++) {
                if(i!=0) *buffer += ",";
                int num = pvdata->get(i,1,&data);
                if(num==1) {
                     int val = data.data[data.offset];
                     char buf[16];
                     sprintf(buf,"%d",val);
                     *buffer += buf;
                } else {
                     *buffer += "???? ";
                }
            }
            *buffer += "]";
            break;
        }
    case pvShort: {
            PVShortArray *pvdata = static_cast<PVShortArray*>(pv);
            ShortArrayData data = ShortArrayData();
            *buffer += "[";
            for(int i=0; i < pvdata->getLength(); i++) {
                if(i!=0) *buffer += ',';
                int num = pvdata->get(i,1,&data);
                if(num==1) {
                     int val = data.data[data.offset];
                     char buf[16];
                     sprintf(buf,"%d",val);
                     *buffer += buf;
                } else {
                     *buffer += "???? ";
                }
            }
            *buffer += "]";
            break;
        }
    case pvInt: {
            PVIntArray *pvdata = static_cast<PVIntArray*>(pv);
            IntArrayData data = IntArrayData();
            *buffer += "[";
            for(int i=0; i < pvdata->getLength(); i++) {
                if(i!=0) *buffer += ',';
                int num = pvdata->get(i,1,&data);
                if(num==1) {
                     int val = data.data[data.offset];
                     char buf[16];
                     sprintf(buf,"%d",val);
                     *buffer += buf;
                } else {
                     *buffer += "???? ";
                }
            }
            *buffer += "]";
            break;
        }
    case pvLong: {
            PVLongArray *pvdata = static_cast<PVLongArray*>(pv);
            LongArrayData data = LongArrayData();
            *buffer += "[";
            for(int i=0; i < pvdata->getLength(); i++) {
                if(i!=0) *buffer += ',';
                int num = pvdata->get(i,1,&data);
                if(num==1) {
                     int64 val = data.data[data.offset];
                     char buf[16];
                     sprintf(buf,"%lld",val);
                     *buffer += buf;
                } else {
                     *buffer += "???? ";
                }
            }
            *buffer += "]";
            break;
        }
    case pvFloat: {
            PVFloatArray *pvdata = static_cast<PVFloatArray*>(pv);
            FloatArrayData data = FloatArrayData();
            *buffer += "[";
            for(int i=0; i < pvdata->getLength(); i++) {
                if(i!=0) *buffer += ',';
                int num = pvdata->get(i,1,&data);
                if(num==1) {
                     float val = data.data[data.offset];
                     char buf[16];
                     sprintf(buf,"%g",val);
                     *buffer += buf;
                } else {
                     *buffer += "???? ";
                }
            }
            *buffer += "]";
            break;
        }
    case pvDouble: {
            PVDoubleArray *pvdata = static_cast<PVDoubleArray*>(pv);
            DoubleArrayData data = DoubleArrayData();
            *buffer += "[";
            for(int i=0; i < pvdata->getLength(); i++) {
                if(i!=0) *buffer += ',';
                int num = pvdata->get(i,1,&data);
                if(num==1) {
                     double val = data.data[data.offset];
                     char buf[16];
                     sprintf(buf,"%lg",val);
                     *buffer += buf;
                } else {
                     *buffer += "???? ";
                }
            }
            *buffer += ("]");
            break;
        }
    case pvString: {
    	PVStringArray *pvdata = static_cast<PVStringArray*>(pv);
    	StringArrayData data = StringArrayData();
    	*buffer += "[";
    	for(int i=0; i < pvdata->getLength(); i++) {
    		if(i!=0) *buffer += ",";
    		int num = pvdata->get(i,1,&data);
    		StringArray value = data.data;
                if(num==1) {
                    if(value[data.offset].length()>0) {
                         *buffer += value[data.offset].c_str();
                    } else {
                         *buffer += "null";
                    }
    		} else {
    			*buffer += "null";
    		}
    	}
    	*buffer += "]";
    	break;
    }
    default:
        *buffer += "(array element is unknown ScalarType)";
    }
}

void convertStructureArray(StringBuilder buffer,
    PVStructureArray * pvdata,int notFirst)
{
    int length = pvdata->getLength();
    if(length<=0) {
        return;
    }
    StructureArrayData data = StructureArrayData();
    pvdata->get(0, length, &data);
    for (int i = 0; i < length; i++) {
        PVStructure *pvStructure = data.data[i];
        if (pvStructure == 0) {
            *buffer += "null";
        } else {
            convertToString(buffer,pvStructure,notFirst+1);
        }
    }
}











/* Converts a hex character to its integer value */
char from_hex(char ch) {
  return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* Converts an integer value to its hex character*/
char to_hex(char code) {
  static char hex[] = "0123456789abcdef";
  return hex[code & 15];
}

/* Returns a url-encoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *url_encode(char *str) {
  char *pstr = str, *buf = (char*)malloc(strlen(str) * 3 + 1), *pbuf = buf;
  bool firstEquals = true;
  while (*pstr) {
    if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~') 
      *pbuf++ = *pstr;
    else if (*pstr == ' ') 
      *pbuf++ = '+';
    else if (*pstr == '=' && firstEquals)
    { 
      firstEquals = false;
      *pbuf++ = '=';
    }
    else 
      *pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
    pstr++;
  }
  *pbuf = '\0';
  return buf;
}








void toNTString(StringBuilder buffer,PVField * pv,int notFirst)
{
    Type type = pv->getField()->getType();
    if(type==structure)
    {
        PVStructure* pvStruct = static_cast<PVStructure*>(pv);
        // TODO type check, getStringField is verbose
        PVString* ntType = static_cast<PVString*>(pvStruct->getSubField("normativeType"));
        if (ntType)
        {
            String value = ntType->get();
            
            if (value == "NTTable")
            {
                // TODO
                pv->toString(buffer);
            }
            else
            {
                std::cout << "unsupported normative type" << std::endl;
                pv->toString(buffer);
            }
            
            return;
        }
    }
    
    
    pv->toString(buffer);
}








#define DEFAULT_TIMEOUT 3.0
#define DEFAULT_REQUEST "field(value)"

double timeOut = DEFAULT_TIMEOUT;
string request(DEFAULT_REQUEST);
bool terseMode = false;


void usage (void)
{
    fprintf (stderr, "\nUsage: eget [options] [<PV name>... | -s <service name>]\n\n"
    "  -h: Help: Print this message\n"
    "\noptions:\n"
    "  -s <service name>:   RPC based service name\n"
    "  -p <service param>:  Service parameter in form 'name=value'\n"
    "  -r <pv request>:     Request, specifies what fields to return and options, default is '%s'\n"
    "  -w <sec>:            Wait time, specifies timeout, default is %f second(s)\n"
    "  -t:                  Terse mode - print only value, without name\n"
    "  -d:                  Enable debug output\n"
    "  -c:                  Wait for clean shutdown and report used instance count (for expert users)"
    "\n\nexamples:\n\n"
"#! Get the value of the PV corr:li32:53:bdes\n"
"> eget corr:li32:53:bdes\n"
"\n"
"#! Get the table of all correctors from the rdb service\n"
"> eget -s rdbService -p entity=swissfel:devicenames\n"
"\n"
"#! Get the archive history of quad345:hist between 2 times, from the archive service\n"
"> eget -s archiveService -p entity=quad345:hist -p starttime=2012-02-12T10:04:56 -p endtime=2012-02-01T10:04:56\n"
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
    
    ChannelGetRequesterImpl(String channelName) : m_channelName(channelName) {};
    
    virtual String getRequesterName()
    {
        return "ChannelGetRequesterImpl";
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    virtual void channelGetConnect(const epics::pvData::Status& status,ChannelGet::shared_pointer const & channelGet,
                                   epics::pvData::PVStructure::shared_pointer const & pvStructure, 
                                   epics::pvData::BitSet::shared_pointer const & bitSet)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOk())
            {
                std::cout << "[" << m_channelName << "] channel get create: " << status.toString() << std::endl;
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
            std::cout << "[" << m_channelName << "] failed to create channel get: " << status.toString() << std::endl;
        }
    }

    virtual void getDone(const epics::pvData::Status& status)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOk())
            {
                std::cout << "[" << m_channelName << "] channel get: " << status.toString() << std::endl;
            }

            String str;
            
            // access smart pointers
            {
                Lock lock(m_pointerMutex);
                {
                    // needed since we access the data
                    ScopedLock dataLock(m_channelGet);
    
                    if (terseMode)
                        convertToString(&str, m_pvStructure.get(), 0);
                    else
                        m_pvStructure->toString(&str);
                } 
                // this is OK since calle holds also owns it
                m_channelGet.reset();
            }
            
            std::cout << str << std::endl;
            
            m_event.signal();
            
        }
        else
        {
            std::cout << "[" << m_channelName << "] failed to get: " << status.toString() << std::endl;
            {
                Lock lock(m_pointerMutex);
                // this is OK since calle holds also owns it
                m_channelGet.reset();
            }
        }
        
    }

    bool waitUntilGet(double timeOut)
    {
        return m_event.wait(timeOut);
    }
};

class ChannelRPCRequesterImpl : public ChannelRPCRequester
{
    private:
    ChannelRPC::shared_pointer m_channelRPC;
    Mutex m_pointerMutex;
    Event m_event;
    Event m_connectionEvent;
    String m_channelName;

    public:
    
    ChannelRPCRequesterImpl(String channelName) : m_channelName(channelName) {};
    
    virtual String getRequesterName()
    {
        return "ChannelRPCRequesterImpl";
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    virtual void channelRPCConnect(const epics::pvData::Status& status,ChannelRPC::shared_pointer const & channelRPC)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOk())
            {
                std::cout << "[" << m_channelName << "] channel RPC create: " << status.toString() << std::endl;
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
            std::cout << "[" << m_channelName << "] failed to create channel get: " << status.toString() << std::endl;
        }
    }

    virtual void requestDone (const epics::pvData::Status &status, epics::pvData::PVStructure::shared_pointer const &pvResponse)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOk())
            {
                std::cout << "[" << m_channelName << "] channel RPC: " << status.toString() << std::endl;
            }

            String str;
            
            // access smart pointers
            {
                Lock lock(m_pointerMutex);
                {
                    // TODO format normative types
                    if (terseMode)
                        convertToString(&str, pvResponse.get(), 0);
                    else
                        pvResponse->toString(&str);
                } 
                // this is OK since calle holds also owns it
                m_channelRPC.reset();
            }
            
            std::cout << str << std::endl;
            
            m_event.signal();
            
        }
        else
        {
            std::cout << "[" << m_channelName << "] failed to RPC: " << status.toString() << std::endl;
            {
                Lock lock(m_pointerMutex);
                // this is OK since calle holds also owns it
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

class ChannelRequesterImpl : public ChannelRequester
{
private:
    Event m_event;    
    
public:
    
    virtual String getRequesterName()
    {
        return "ChannelRequesterImpl";
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    virtual void channelCreated(const epics::pvData::Status& status, Channel::shared_pointer const & channel)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOk())
            {
                std::cout << "[" << channel->getChannelName() << "] channel create: " << status.toString() << std::endl;
            }
        }
        else
        {
            std::cout << "[" << channel->getChannelName() << "] failed to create a channel: " << status.toString() << std::endl;
        }
    }

    virtual void channelStateChange(Channel::shared_pointer const & channel, Channel::ConnectionState connectionState)
    {
        if (connectionState == Channel::CONNECTED)
        {
            m_event.signal();
        }
        /*
        else if (connectionState != Channel::DESTROYED)
        {
            std::cout << "[" << channel->getChannelName() << "] channel state change: "  << Channel::ConnectionStateNames[connectionState] << std::endl;
        }
        */
    }
    
    bool waitUntilConnected(double timeOut)
    {
        return m_event.wait(timeOut);
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
    string service;
    string urlEncodedRequest;
    vector< pair<string,string> > parameters;

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */

    while ((opt = getopt(argc, argv, ":hr:s:p:w:tdc")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'w':               /* Set CA timeout value */
            if(epicsScanDouble(optarg, &timeOut) != 1)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('cainfo -h' for help.)\n", optarg);
                timeOut = DEFAULT_TIMEOUT;
            }
            break;
        case 'r':               /* Set timeout value */
            request = optarg;
            break;          
        case 'p':               /* Servie parameters */
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
        case 't':               /* Terse mode */
            terseMode = true;
            break;
        case 'd':               /* Debug log level */
            debug = true;
            break;
        case 'c':               /* Clean-up and report used instance count */
            cleanupAndReport = true;
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

    bool allOK = true;


    // PVs mode
    if (!serviceRequest)
    {
        vector<string> pvs;     /* Array of PV structures */
        for (int n = 0; optind < argc; n++, optind++)
            pvs.push_back(argv[optind]);       /* Copy PV names from command line */
        
        PVStructure::shared_pointer pvRequest;
        try {
            pvRequest = getCreateRequest()->createRequest(request);
        } catch (std::exception &ex) {
            printf("failed to parse request string: %s\n", ex.what());
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
                shared_ptr<ChannelGetRequesterImpl> getRequesterImpl(new ChannelGetRequesterImpl(channel->getChannelName()));
                ChannelGet::shared_pointer channelGet = channel->createChannelGet(getRequesterImpl, pvRequest);
                allOK &= getRequesterImpl->waitUntilGet(timeOut);
            }
            else
            {
                allOK = false;
                channel->destroy();
                std::cout << "[" << channel->getChannelName() << "] connection timeout" << std::endl;
            }
        }    
    
        ClientFactory::stop();
    }
    // service RPC mode
    else
    {
        std::cout << "service            : " << service << std::endl;
        std::cout << "parameters         : " << std::endl;

        vector< pair<string, string> >::iterator iter = parameters.begin();
        for (; iter != parameters.end(); iter++)
            std::cout << "    " << iter->first << " = " << iter->second << std::endl;
        std::cout << "encoded URL request: '" << urlEncodedRequest << "'" << std::endl;
        
        // TODO simply empty?
        PVStructure::shared_pointer pvRequest;
        try {
            pvRequest = getCreateRequest()->createRequest(request);
        } catch (std::exception &ex) {
            printf("failed to parse request string: %s\n", ex.what());
            return 1;
        }
        
        int i = 0;
        FieldConstPtrArray fields = new FieldConstPtr[parameters.size()];
        for (vector< pair<string, string> >::iterator iter = parameters.begin();
             iter != parameters.end();
             iter++, i++)
        {
            fields[i] = getFieldCreate()->createScalar(iter->first, pvString);
        }
        PVStructure::shared_pointer args(
            new PVStructure(NULL, getFieldCreate()->createStructure("", parameters.size(), fields)));
        for (vector< pair<string, string> >::iterator iter = parameters.begin();
             iter != parameters.end();
             iter++)
        {
            args->getStringField(iter->first)->put(iter->second);
        }

        ClientFactory::start();
        ChannelProvider::shared_pointer provider = getChannelAccess()->getProvider("pvAccess");
        
        shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl()); 
        Channel::shared_pointer channel = provider->createChannel(service, channelRequesterImpl);
        
        if (channelRequesterImpl->waitUntilConnected(timeOut))
        {
            shared_ptr<ChannelRPCRequesterImpl> getRequesterImpl(new ChannelRPCRequesterImpl(channel->getChannelName()));
            ChannelRPC::shared_pointer channelRPC = channel->createChannelRPC(getRequesterImpl, pvRequest);
            
            channelRPC->request(args, true);
            allOK &= getRequesterImpl->waitUntilRPC(timeOut);
        }
        else
        {
            allOK = false;
            channel->destroy();
            std::cout << "[" << channel->getChannelName() << "] connection timeout" << std::endl;
        }
    
        ClientFactory::stop();
    }

    if (cleanupAndReport)
    {
        // TODO implement wait on context
        epicsThreadSleep ( 3.0 );
        std::cout << "-----------------------------------------------------------------------" << std::endl;
        epicsExitCallAtExits();
        CDRMonitor::get().show(stdout, true);
    }

    return allOK ? 0 : 1;
}
