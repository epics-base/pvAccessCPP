#include <iostream>
#include <clientFactory.h>

#include <stdio.h>
#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <errlog.h>

#include <vector>
#include <string>

using namespace std;
using namespace std::tr1;
using namespace epics::pvData;
using namespace epics::pvAccess;





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



















#define DEFAULT_TIMEOUT 3.0
#define DEFAULT_REQUEST "field(value)"

double timeOut = DEFAULT_TIMEOUT;
string request(DEFAULT_REQUEST);
bool terseMode = false;

PVStructure::shared_pointer pvRequest;

void usage (void)
{
    fprintf (stderr, "\nUsage: pvaget [options] <PV name>...\n\n"
    "  -h: Help: Print this message\n"
    "options:\n"
    "  -r <pv request>:   Request, specifies what fields to return and options, default is '%s'\n"
    "  -w <sec>:          Wait time, specifies timeout, default is %f second(s)\n"
    "  -t:                Terse mode - print only value, without name"
    "\nExample: pvaget example001 \n\n"
             , DEFAULT_REQUEST, DEFAULT_TIMEOUT);
}


class ChannelGetRequesterImpl : public ChannelGetRequester
{
    private:
    PVStructure::shared_pointer m_pvStructure;
    BitSet::shared_pointer m_bitSet;
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
            if (!status.isOK())
            {
                std::cout << "[" << m_channelName << "] channel get create: " << status.toString() << std::endl;
            }

            m_pvStructure = pvStructure;
            m_bitSet = bitSet;
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
            if (!status.isOK())
            {
                std::cout << "[" << m_channelName << "] channel get: " << status.toString() << std::endl;
            }

            String str;
            
            if (terseMode)
                convertToString(&str, m_pvStructure.get(), 0);
            else
                m_pvStructure->toString(&str);
            

            std::cout << str << std::endl;
            
            m_event.signal();
        }
        else
        {
            std::cout << "[" << m_channelName << "] failed to get: " << status.toString() << std::endl;
        }
        
    }

    bool waitUntilGet(double timeOut)
    {
        return m_event.wait(timeOut);
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
            if (!status.isOK())
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
 * Description:	pvaget main()
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

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */

    while ((opt = getopt(argc, argv, ":hr:w:t")) != -1) {
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
        case 'r':               /* Set CA timeout value */
            request = optarg;
            break;
        case 't':               /* Terse mode */
            terseMode = true;
            break;
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('pvaget -h' for help.)\n",
                    optopt);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('pvaget -h' for help.)\n",
                    optopt);
            return 1;
        default :
            usage();
            return 1;
        }
    }

    int nPvs = argc - optind;       /* Remaining arg list are PV names */
    if (nPvs < 1)
    {
        fprintf(stderr, "No pv name specified. ('pvaget -h' for help.)\n");
        return 1;
    }

    vector<string> pvs;     /* Array of PV structures */
    for (int n = 0; optind < argc; n++, optind++)
        pvs.push_back(argv[optind]);       /* Copy PV names from command line */

    try {
        pvRequest = getCreateRequest()->createRequest(request);
    } catch (std::exception &ex) {
        printf("failed to parse request string: %s\n", ex.what());
        return 1;
    }
    
    // typedef enum {errlogInfo, errlogMinor, errlogMajor, errlogFatal} errlogSevEnum;
    errlogSetSevToLog(errlogMajor);


    ClientFactory::start();
    ChannelProvider::shared_pointer provider = getChannelAccess()->getProvider("pvAccess");

    bool allOK = true;
    
    // for now a simple iterating sync implementation, guarantees order
    for (int n = 0; n < nPvs; n++)
    {
        shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl()); 
        Channel::shared_pointer channel = provider->createChannel(pvs[n], channelRequesterImpl);
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

    return 0;
}
