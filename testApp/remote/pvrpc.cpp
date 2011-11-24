#include <iostream>
#include <pv/clientFactory.h>

#include <stdio.h>
#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <pv/logger.h>

#include <vector>
#include <string>

#include <pv/convert.h>
#include <pv/event.h>

using namespace std;
using namespace std::tr1;
using namespace epics::pvData;
using namespace epics::pvAccess;

#define DEFAULT_TIMEOUT 3.0
#define DEFAULT_REQUEST "field(value)"

double timeOut = DEFAULT_TIMEOUT;
string request(DEFAULT_REQUEST);
bool terseMode = false;

PVStructure::shared_pointer pvRequest;

void usage ()
{
    fprintf (stderr, "\nUsage: pvrpc [options] <PV name> <values>...\n\n"
    "  -h: Help: Print this message\n"
    "options:\n"
    "  -r <pv request>:   Request, specifies what fields to return and options, default is '%s'\n"
    "  -w <sec>:          Wait time, specifies timeout, default is %f second(s)\n"
    "  -t:                Terse mode - print only value, without name\n"
    "  -d:                Enable debug output"
    "\nExample: pvrpc example001 1.234 10 test\n\n"
             , DEFAULT_REQUEST, DEFAULT_TIMEOUT);
}


class ChannelRPCRequesterImpl : public ChannelRPCRequester
{
    auto_ptr<Event> m_event;
    public:
    shared_ptr<epics::pvData::PVStructure> pvResponse;
    ChannelRPCRequesterImpl(String channelName) 
    {
        resetEvent();
    }

    virtual void channelRPCConnect (const epics::pvData::Status &status, ChannelRPC::shared_pointer const &channelRPC)
    {
        m_event->signal();
    }

    virtual void requestDone (const epics::pvData::Status &status, epics::pvData::PVStructure::shared_pointer const &pvResponse)
    {
        this->pvResponse = pvResponse;
        m_event->signal();
    }

    virtual String getRequesterName()
    {
        return "ChannelRPCRequesterImpl";
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    void resetEvent()
    {
        m_event.reset(new Event());
    }

    bool waitUntilDone(double timeOut)
    {
        return m_event->wait(timeOut);
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
 * Description:	pvrpc main()
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
        case 'd':               /* Debug log level */
            debug = true;
            break;
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('pvrpc -h' for help.)\n",
                    optopt);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('pvrpc -h' for help.)\n",
                    optopt);
            return 1;
        default :
            usage();
            return 1;
        }
    }

    if (argc <= optind)
    {
        fprintf(stderr, "No pv name specified. ('pvrpc -h' for help.)\n");
        return 1;
    }
    string pvName = argv[optind++];
    

    int nVals = argc - optind;       /* Remaining arg list are PV names */
    if (nVals < 1)
    {
        fprintf(stderr, "No value(s) specified. ('pvrpc -h' for help.)\n");
        return 1;
    }

    vector<string> values;     /* Array of values */
    for (int n = 0; optind < argc; n++, optind++)
        values.push_back(argv[optind]);       /* Copy values from command line */

    try {
        pvRequest = getCreateRequest()->createRequest(request);
    } catch (std::exception &ex) {
        printf("failed to parse request string: %s\n", ex.what());
        return 1;
    }
    
    SET_LOG_LEVEL(debug ? logLevelDebug : logLevelError);


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
                shared_ptr<ChannelRPCRequesterImpl> rpcRequesterImpl(new ChannelRPCRequesterImpl(channel->getChannelName()));
                
                // A PVStructure is sent at ChannelRPC connect time but we don't use it, so send an empty one
                PVStructure::shared_pointer nothing(
                    new PVStructure(NULL, getFieldCreate()->createStructure("nothing", 0, NULL)));

                ChannelRPC::shared_pointer channelRPC = channel->createChannelRPC(rpcRequesterImpl, nothing);
                allOK &= rpcRequesterImpl->waitUntilDone(timeOut);
                std::cout << "connected" << std::endl;
                if (allOK)
                {
                    rpcRequesterImpl->resetEvent();
                    channelRPC->request(pvRequest, false);
                    allOK &= rpcRequesterImpl->waitUntilDone(timeOut);
                    if (allOK)
                    {
                        String s;
                        rpcRequesterImpl->pvResponse->toString(&s);
                        std::cout << s << std::endl;
                    }
                    else
                    {
                        std::cout << "Error" << std::endl;
                    }
                }
            }
            else
            {
                allOK = false;
                channel->destroy();
                std::cout << "[" << channel->getChannelName() << "] connection timeout" << std::endl;
                break;
            }
        }
        while (false);
    } catch (std::out_of_range& oor) {
        allOK = false;
        std::cout << "parse error: not enough of values" << std::endl;
    } catch (std::exception& ex) {
        allOK = false;
        std::cout << ex.what() << std::endl;
    } catch (...) {
        allOK = false;
        std::cout << "unknown exception caught" << std::endl;
    }
        
    ClientFactory::stop();

    return allOK ? 0 : 1;
}
