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

//PVStructure::shared_pointer pvRequest;

void usage (void)
{
    fprintf (stderr, "\nUsage: pvput [options] <PV name> <values>...\n\n"
    "  -h: Help: Print this message\n"
    "options:\n"
    "  -r <pv request>:   Request, specifies what fields to return and options, default is '%s'\n"
    "  -w <sec>:          Wait time, specifies timeout, default is %f second(s)\n"
    "  -t:                Terse mode - print only successfully written value, without names\n"
    "  -d:                Enable debug output\n"
    "  -F <ofs>:          Use <ofs> as an alternate output field separator"
    "\nExample: pvput double01 1.234\n\n"
             , DEFAULT_REQUEST, DEFAULT_TIMEOUT);
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
    AtomicBoolean m_supressGetValue;

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
                std::cerr << "[" << m_channelName << "] channel put create: " << status.toString() << std::endl;
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
            std::cerr << "[" << m_channelName << "] failed to create channel put: " << status.toString() << std::endl;
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
            // do not print old value in terseMode
            if (!m_supressGetValue.get())
            {
                Lock lock(m_pointerMutex);
                {
                    // needed since we access the data
                    ScopedLock dataLock(m_channelPut);

                    if (mode == ValueOnlyMode)
                    {
                        PVField::shared_pointer value = m_pvStructure->getSubField("value");
                        if (value.get() == 0)
                        {
                        	std::cerr << "no 'value' field" << std::endl;
                            return;
                        }

                        if (fieldSeparator == ' ' && value->getField()->getType() == scalar)
                        	std::cout << std::setw(30) << std::left << m_channelName;
                        else
                        	std::cout << m_channelName;

                        std::cout << fieldSeparator;

                        terse(std::cout, value) << std::endl;
                    }
                    else if (mode == TerseMode)
                        terseStructure(std::cout, m_pvStructure) << std::endl;
                    else
                        std::cout << std::endl << *(m_pvStructure.get()) << std::endl << std::endl;
                }
            }
            
            m_event->signal();
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to get: " << status.toString() << std::endl;
        }
        
    }

    virtual void putDone(const epics::pvData::Status& status)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel put: " << status.toString() << std::endl;
            }
  
            m_event->signal();
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to get: " << status.toString() << std::endl;
        }
        
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
    }
    
    bool waitUntilDone(double timeOut)
    {
    	Event* event;
    	{
    		Lock lock(m_eventMutex);
    		event = m_event.get();
    	}
        return event->wait(timeOut);
    }

    void supressGetValue(bool flag)
    {
    	if (flag)
    		m_supressGetValue.set();
    	else
    		m_supressGetValue.clear();
    }
};

/*+**************************************************************************
 *
 * Function:	main
 *
 * Description:	pvput main()
 * 		Evaluate command line options, set up CA, connect the
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

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */

    while ((opt = getopt(argc, argv, ":hr:w:tdF:")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'w':               /* Set CA timeout value */
            if(epicsScanDouble(optarg, &timeOut) != 1)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('pvput -h' for help.)\n", optarg);
                timeOut = DEFAULT_TIMEOUT;
            }
            break;
        case 'r':               /* Set CA timeout value */
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

    bool allOK = true;

    try
    {
        do
        {
            // first connect
            shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl()); 
            Channel::shared_pointer channel = provider->createChannel(pvName, channelRequesterImpl);
            
            if (channelRequesterImpl->waitUntilConnected(timeOut))
            {
                shared_ptr<ChannelPutRequesterImpl> putRequesterImpl(new ChannelPutRequesterImpl(channel->getChannelName()));
                if (mode == TerseMode)
                	putRequesterImpl->supressGetValue(true);
                else
                	std::cout << "Old : ";
                ChannelPut::shared_pointer channelPut = channel->createChannelPut(putRequesterImpl, pvRequest);
                allOK &= putRequesterImpl->waitUntilDone(timeOut);
                if (allOK)
                {
                	// convert value from string
                	// since we access structure from another thread, we need to lock
                	{
						ScopedLock lock(channelPut);
						// TODO SIGSEG
						getConvert()->fromString(putRequesterImpl->getStructure(), values);
                	}

                    // we do a put
                    putRequesterImpl->resetEvent();
                    channelPut->put(false);
                    allOK &= putRequesterImpl->waitUntilDone(timeOut);
        
                    if (allOK)
                    {
                        // and than a get again to verify put
                        if (mode != TerseMode) std::cout << "New : ";
                        putRequesterImpl->supressGetValue(false);
                        putRequesterImpl->resetEvent();
                        channelPut->get();
                        allOK &= putRequesterImpl->waitUntilDone(timeOut);
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
