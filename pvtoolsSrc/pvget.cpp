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
#include <istream>
#include <fstream>
#include <sstream>

#include <pv/event.h>
#include <epicsExit.h>

#include "pvutils.cpp"

#include <pv/caProvider.h>

using namespace std;
using namespace std::tr1;
using namespace epics::pvData;
using namespace epics::pvAccess;






#define DEFAULT_TIMEOUT 3.0
#define DEFAULT_REQUEST "field(value)"

double timeOut = DEFAULT_TIMEOUT;
string request(DEFAULT_REQUEST);

enum PrintMode { ValueOnlyMode, StructureMode, TerseMode };
PrintMode mode = ValueOnlyMode;

char fieldSeparator = ' ';

void usage (void)
{
    fprintf (stderr, "\nUsage: pvget [options] <PV name>...\n\n"
    "  -h: Help: Print this message\n"
    "options:\n"
    "  -r <pv request>:   Request, specifies what fields to return and options, default is '%s'\n"
    "  -w <sec>:          Wait time, specifies timeout, default is %f second(s)\n"
    "  -t:                Terse mode - print only value, without names\n"
    "  -i:                Do not format standard types (enum_t, time_t, ...)\n"
    "  -m:                Monitor mode\n"
    "  -q:                Quiet mode, print only error messages\n"
    "  -d:                Enable debug output\n"
    "  -F <ofs>:          Use <ofs> as an alternate output field separator\n"
    "  -f <input file>:   Use <input file> as an input that provides a list PV name(s) to be read, use '-' for stdin\n"
    "  -c:                Wait for clean shutdown and report used instance count (for expert users)\n"
    " enum format:\n"
    "  -n: Force enum interpretation of values as numbers (default is enum string)\n"
//    " time format:\n"
//    "  -u: print userTag\n"
    "\nexample: pvget double01\n\n"
             , DEFAULT_REQUEST, DEFAULT_TIMEOUT);
}

void printValue(std::string const & channelName, PVStructure::shared_pointer const & pv)
{
    if (mode == ValueOnlyMode)
    {
        PVField::shared_pointer value = pv->getSubField("value");
        if (value.get() == 0)
        {
        	std::cerr << "no 'value' field" << std::endl;
            //std::cout << channelName << std::endl << *(pv.get()) << std::endl << std::endl;
            pvutil_ostream myos(std::cout.rdbuf());
            myos << channelName << std::endl << *(pv.get()) << std::endl << std::endl;
        }
        else
        {
			Type valueType = value->getField()->getType();
			if (valueType != scalar && valueType != scalarArray)
			{
                // switch to structure mode, unless it's T-type
                if (valueType == structure && isTType(static_pointer_cast<PVStructure>(value)))
                {
                    std::cout << std::setw(30) << std::left << channelName;
                    std::cout << fieldSeparator;
                    formatTType(std::cout, static_pointer_cast<PVStructure>(value));
                    std::cout << std::endl;
                }
                else
                {
                    //std::cout << channelName << std::endl << *(pv.get()) << std::endl << std::endl;
                    pvutil_ostream myos(std::cout.rdbuf());
                    myos << channelName << std::endl << *(pv.get()) << std::endl << std::endl;
                }
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
    {
        //std::cout << channelName << std::endl << *(pv.get()) << std::endl << std::endl;
        pvutil_ostream myos(std::cout.rdbuf());
        myos << channelName << std::endl << *(pv.get()) << std::endl << std::endl;
    }
}


class ChannelGetRequesterImpl : public ChannelGetRequester
{
    private:
    PVStructure::shared_pointer m_pvStructure;
    BitSet::shared_pointer m_bitSet;
    Mutex m_pointerMutex;
    Event m_event;
    string m_channelName;

    bool m_done;

    public:
    
    ChannelGetRequesterImpl(std::string channelName) : m_channelName(channelName), m_done(false) {}
    
    virtual string getRequesterName()
    {
        return "ChannelGetRequesterImpl";
    }

    virtual void message(std::string const & message, MessageType messageType)
    {
        std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelGetConnect(const epics::pvData::Status& status, ChannelGet::shared_pointer const & channelGet,
                                   epics::pvData::Structure::const_shared_pointer const & /*structure*/)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel get create: " << status << std::endl;
            }
            
            channelGet->lastRequest();
            channelGet->get();
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to create channel get: " << status << std::endl;
            m_event.signal();
        }
    }

    virtual void getDone(const epics::pvData::Status& status, 
            ChannelGet::shared_pointer const & /*channelGet*/,
            epics::pvData::PVStructure::shared_pointer const & pvStructure, 
            epics::pvData::BitSet::shared_pointer const & bitSet)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel get: " << status << std::endl;
            }

            // access smart pointers
            {
                Lock lock(m_pointerMutex);
                
                m_pvStructure = pvStructure;
                m_bitSet = bitSet;

                m_done = true;

            }
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to get: " << status << std::endl;
        }
        
        m_event.signal();
    }

    PVStructure::shared_pointer getPVStructure()
    {
        Lock lock(m_pointerMutex);
        return m_pvStructure;
    }

    bool waitUntilGet(double timeOut)
    {
    	bool signaled = m_event.wait(timeOut);
    	if (!signaled)
    	{
            std::cerr << "[" << m_channelName << "] get timeout" << std::endl;
            return false;
    	}

		Lock lock(m_pointerMutex);
		return m_done;
    }
};

class MonitorRequesterImpl : public MonitorRequester
{
	private:

    string m_channelName;

    public:

    MonitorRequesterImpl(std::string channelName) : m_channelName(channelName) {};

    virtual string getRequesterName()
    {
        return "MonitorRequesterImpl";
    };

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void monitorConnect(const epics::pvData::Status& status, Monitor::shared_pointer const & monitor, StructureConstPtr const & /*structure*/)
    {
        if (status.isSuccess())
        {
        	/*
            string str;
            structure->toString(&str);
            std::cout << str << std::endl;
        	*/

            Status startStatus = monitor->start();
            // show error
            // TODO and exit
            if (!startStatus.isSuccess())
            {
                std::cerr << "[" << m_channelName << "] channel monitor start: " << startStatus << std::endl;
            }

        }
        else
        {
            std::cerr << "monitorConnect(" << status << ")" << std::endl;
        }
    }

    virtual void monitorEvent(Monitor::shared_pointer const & monitor)
    {

		MonitorElement::shared_pointer element;
        while ((element = monitor->poll()))
		{
            if (mode == ValueOnlyMode)
            {
                PVField::shared_pointer value = element->pvStructurePtr->getSubField("value");
                if (value.get() == 0)
                {
                	std::cerr << "no 'value' field" << std::endl;
                	std::cout << m_channelName << std::endl;
                    //std::cout << *(element->pvStructurePtr.get()) << std::endl << std::endl;
                    pvutil_ostream myos(std::cout.rdbuf());
                    myos << *(element->pvStructurePtr.get()) << std::endl << std::endl;
                }
                else
                {
					Type valueType = value->getField()->getType();
					if (valueType != scalar && valueType != scalarArray)
					{
                        // switch to structure mode, unless it's T-type
                        if (valueType == structure && isTType(static_pointer_cast<PVStructure>(value)))
                        {
                            std::cout << std::setw(30) << std::left << m_channelName;
                            std::cout << fieldSeparator;
                            formatTType(std::cout, static_pointer_cast<PVStructure>(value));
                            std::cout << std::endl;
                        }
                        else
                        {
                            std::cout << m_channelName << std::endl;
                            //std::cout << *(element->pvStructurePtr.get()) << std::endl << std::endl;
                            pvutil_ostream myos(std::cout.rdbuf());
                            myos << *(element->pvStructurePtr.get()) << std::endl << std::endl;
                        }
                    }
					else
					{
						if (fieldSeparator == ' ' && value->getField()->getType() == scalar)
							std::cout << std::setw(30) << std::left << m_channelName;
						else
							std::cout << m_channelName;

						std::cout << fieldSeparator;

						terse(std::cout, value) << std::endl;
					}
                }
            }
            else if (mode == TerseMode)
            {
            	if (fieldSeparator == ' ')
                	std::cout << std::setw(30) << std::left << m_channelName;
                else
                	std::cout << m_channelName;

                std::cout << fieldSeparator;

                terseStructure(std::cout, element->pvStructurePtr) << std::endl;
            }
            else
            {
            	std::cout << m_channelName << std::endl;
                //std::cout << *(element->pvStructurePtr.get()) << std::endl << std::endl;
                pvutil_ostream myos(std::cout.rdbuf());
                myos << *(element->pvStructurePtr.get()) << std::endl << std::endl;
            }

			monitor->release(element);
		}

    }

    virtual void unlisten(Monitor::shared_pointer const & /*monitor*/)
    {
        std::cerr << "unlisten" << std::endl;
    }
};



/*+**************************************************************************
 *
 * Function:	main
 *
 * Description:	pvget main()
 * 		Evaluate command line options, set up PVA, connect the
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
    bool monitor = false;
    bool quiet = false;

    istream* inputStream = 0;
    ifstream ifs;
    bool fromStream = false;

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */

    while ((opt = getopt(argc, argv, ":hr:w:tmqdcF:f:ni")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'w':               /* Set PVA timeout value */
            if(epicsScanDouble(optarg, &timeOut) != 1 || timeOut <= 0.0)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('pvget -h' for help.)\n", optarg);
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
        case 'i':               /* T-types format mode */
            formatTTypes(false);
            break;
        case 'm':               /* Monitor mode */
            monitor = true;
            break;
        case 'q':               /* Quiet mode */
            quiet = true;
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
        case 'f':               /* Use input stream as input */
        {
            string fileName = optarg;
            if (fileName == "-")
                inputStream = &cin;
            else
            {
                ifs.open(fileName.c_str(), ifstream::in);
                if (!ifs)
                {
                    fprintf(stderr,
                            "Failed to open file '%s'.\n",
                            fileName.c_str());
                    return 1;
                }
                else
                    inputStream = &ifs;
            }

            fromStream = true;
            break;
        }
        case 'n':
            setEnumPrintMode(NumberEnum);
            break;
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('pvget -h' for help.)\n",
                    optopt);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('pvget -h' for help.)\n",
                    optopt);
            return 1;
        default :
            usage();
            return 1;
        }
    }

    int nPvs = argc - optind;       /* Remaining arg list are PV names */
    if (nPvs > 0)
    {
        // do not allow reading file and command line specified pvs
        fromStream = false;
    }
    else if (nPvs < 1 && !fromStream)
    {
        fprintf(stderr, "No pv name(s) specified. ('pvget -h' for help.)\n");
        return 1;
    }

    vector<string> pvs;     /* Array of PV structures */
    if (fromStream)
    {
        string cn;
        while (true)
        {
            *inputStream >> cn;
            if (!(*inputStream))
                break;
            pvs.push_back(cn);
        }

        // set nPvs
        nPvs = pvs.size();
    }
    else
    {
        // copy PV names from command line
        for (int n = 0; optind < argc; n++, optind++)
            pvs.push_back(argv[optind]);
    }


    SET_LOG_LEVEL(debug ? logLevelDebug : logLevelError);

    std::cout << std::boolalpha;
    terseSeparator(fieldSeparator);

    bool allOK = true;

    {
        Requester::shared_pointer requester(new RequesterImpl("pvget"));
    
        PVStructure::shared_pointer pvRequest = CreateRequest::create()->createRequest(request);
        if(pvRequest.get()==NULL) {
            fprintf(stderr, "failed to parse request string\n");
            return 1;
        }
        
        ClientFactory::start();
        ChannelProvider::shared_pointer provider = getChannelProviderRegistry()->getProvider("pva");

        //epics::pvAccess::ca::CAClientFactory::start();
        //ChannelProvider::shared_pointer provider = getChannelProviderRegistry()->getProvider("ca");

        // first connect to all, this allows resource (e.g. TCP connection) sharing
        vector<Channel::shared_pointer> channels(nPvs);
        for (int n = 0; n < nPvs; n++)
        {
            shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl(quiet));
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
							pvRequest = CreateRequest::create()->createRequest("field()");
						}
            		}

					if (!monitor)
					{
						shared_ptr<ChannelGetRequesterImpl> getRequesterImpl(new ChannelGetRequesterImpl(channel->getChannelName()));
						ChannelGet::shared_pointer channelGet = channel->createChannelGet(getRequesterImpl, pvRequest);
						allOK &= getRequesterImpl->waitUntilGet(timeOut);
						if (allOK)
	                    	printValue(channel->getChannelName(), getRequesterImpl->getPVStructure());
					}
					else
					{
                        shared_ptr<ChannelRequesterImpl> channelRequesterImpl = dynamic_pointer_cast<ChannelRequesterImpl>(channel->getChannelRequester());
                        channelRequesterImpl->showDisconnectMessage();

                        shared_ptr<MonitorRequesterImpl> monitorRequesterImpl(new MonitorRequesterImpl(channel->getChannelName()));
						Monitor::shared_pointer monitorGet = channel->createMonitor(monitorRequesterImpl, pvRequest);
						allOK &= true;
					}
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

        if (allOK && monitor)
        {
        	while (true)
        		epicsThreadSleep(timeOut);
        }

        ClientFactory::stop();
    }

    if (cleanupAndReport)
    {
        // TODO implement wait on context
        epicsThreadSleep ( 3.0 );
        //std::cout << "-----------------------------------------------------------------------" << std::endl;
        //epicsExitCallAtExits();
    }

    return allOK ? 0 : 1;
}
