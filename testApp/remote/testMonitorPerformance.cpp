#include <iostream>
#include <fstream>
#include <pv/clientFactory.h>
#include <pv/pvAccess.h>

#include <stdio.h>
#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <pv/logger.h>
#include <pv/lock.h>

#include <vector>
#include <string>

#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/time.h>
#endif

#include <pv/event.h>

using namespace std;
namespace TR1 = std::tr1;
using namespace epics::pvData;
using namespace epics::pvAccess;

#define DEFAULT_TIMEOUT 600.0
#define DEFAULT_REQUEST "record[velocious=true]field(value)"
#define DEFAULT_ITERATIONS 10000
#define DEFAULT_CHANNELS 1
#define DEFAULT_ARRAY_SIZE 0
#define DEFAULT_RUNS 1

bool verbose = false;

int iterations = DEFAULT_ITERATIONS;
int channels = DEFAULT_CHANNELS;
int runs = DEFAULT_RUNS;
int arraySize = DEFAULT_ARRAY_SIZE;          // 0 means scalar
Mutex waitLoopPtrMutex;
TR1::shared_ptr<Event> waitLoopEvent;

double timeOut = DEFAULT_TIMEOUT;
string request(DEFAULT_REQUEST);

PVStructure::shared_pointer pvRequest;

class RequesterImpl : public Requester,
    public TR1::enable_shared_from_this<RequesterImpl>
{
public:

    virtual string getRequesterName()
    {
        return "RequesterImpl";
    };

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }
};

void usage (void)
{
    fprintf (stderr, "\nUsage: testMonitorPerformance [options] <PV name>...\n\n"
             "  -h: Help: Print this message\n"
             "options:\n"
             "  -r <pv request>:   pvRequest string, specifies what fields to return and options, default is '%s'\n"
             "  -i <iterations>:   number of iterations per each run, default is '%d'\n"
             "  -c <channels>:     number of channels, default is '%d'\n"
             "  -s <array size>:   number of array elements (0 means scalar), default is '%d'\n"
             "  -l <runs>:         number of runs (0 means execute runs continuously), default is '%d'\n"
             "  -f <filename>:     read configuration file that contains list of tests to be performed\n"
             "                         each test is defined by a \"<c> <s> <i> <l>\" line\n"
             "                         output is a space separated list of get operations per second for each run, one line per test\n"
             "  -v                 enable verbose output when configuration is read from the file\n"
             "  -w <sec>:          wait time, specifies timeout, default is %f second(s)\n\n"
             , DEFAULT_REQUEST, DEFAULT_ITERATIONS, DEFAULT_CHANNELS, DEFAULT_ARRAY_SIZE, DEFAULT_RUNS, DEFAULT_TIMEOUT);
}

// TODO thread-safety
ChannelProvider::shared_pointer provider;
vector<Monitor::shared_pointer> channelMonitorList;
int channelCount = 0;
int iterationCount = 0;
int runCount = 0;
double sum = 0;

void reset()
{
    channelMonitorList.clear();
    channelCount = 0;
    iterationCount = 0;
    runCount = 0;
    sum = 0;
}

epicsTimeStamp startTime;

void monitor_all()
{
    for (vector<Monitor::shared_pointer>::const_iterator i = channelMonitorList.begin();
            i != channelMonitorList.end();
            i++)
        (*i)->start();
}


// NOTE: it is assumed that all the callbacks are called from the same thread, i.e. same TCP connection
class ChannelMonitorRequesterImpl : public MonitorRequester
{
private:
    Event m_event;
    Event m_connectionEvent;
    string m_channelName;

public:

    ChannelMonitorRequesterImpl(std::string channelName) :
        m_channelName(channelName)
    {
    }

    virtual string getRequesterName()
    {
        return "ChannelMonitorRequesterImpl";
    }

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void monitorConnect(const epics::pvData::Status& status,
                                Monitor::shared_pointer const & /*monitor*/,
                                epics::pvData::Structure::const_shared_pointer const & /*structure*/)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cout << "[" << m_channelName << "] channel monitor create: " << status << std::endl;
            }

            m_connectionEvent.signal();
        }
        else
        {
            std::cout << "[" << m_channelName << "] failed to create channel monitor: " << status << std::endl;
        }
    }

    virtual void monitorEvent(Monitor::shared_pointer const & monitor)
    {

        MonitorElement::shared_pointer element;
        while ((element = monitor->poll()))
        {
            channelCount++;
            if (channelCount == channels)
            {
                iterationCount++;
                channelCount = 0;
            }

            if (iterationCount == iterations)
            {
                epicsTimeStamp endTime;
                epicsTimeGetCurrent(&endTime);

                double duration = epicsTime(endTime) - epicsTime(startTime);
                double getPerSec = iterations*channels/duration;
                double gbit = getPerSec*arraySize*sizeof(double)*8/(1000*1000*1000); // * bits / giga; NO, it's really 1000 and not 1024
                if (verbose)
                    printf("%5.6f seconds, %.3f (x %d = %.3f) monitors/s, data throughput %5.3f Gbits/s\n",
                           duration, iterations/duration, channels, getPerSec, gbit);
                sum += getPerSec;

                iterationCount = 0;
                epicsTimeGetCurrent(&startTime);

                runCount++;
                if (runs == 0 || runCount < runs)
                {
                    // noop
                }
                else
                {
                    printf("%d %d %d %d %.3f\n", channels, arraySize, iterations, runs, sum/runs);

                    Lock guard(waitLoopPtrMutex);
                    waitLoopEvent->signal();    // all done
                }
            }
            else if (channelCount == 0)
            {
                // noop
            }

            monitor->release(element);
        }

    }

    virtual void unlisten(Monitor::shared_pointer const & /*monitor*/)
    {
        std::cerr << "unlisten" << std::endl;
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

    virtual string getRequesterName()
    {
        return "ChannelRequesterImpl";
    };

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelCreated(const epics::pvData::Status& status,
                                Channel::shared_pointer const & channel)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cout << "[" << channel->getChannelName() << "] channel create: " << status << std::endl;
            }
        }
        else
        {
            std::cout << "[" << channel->getChannelName() << "] failed to create a channel: " << status << std::endl;
        }
    }

    virtual void channelStateChange(Channel::shared_pointer const & /*channel*/, Channel::ConnectionState connectionState)
    {
        if (connectionState == Channel::CONNECTED)
        {
            m_event.signal();
        }
        else
        {
            // ups... connection loss
            std::cout << Channel::ConnectionStateNames[connectionState] << std::endl;
            exit(3);
        }
    }

    bool waitUntilConnected(double timeOut)
    {
        return m_event.wait(timeOut);
    }
};

void runTest()
{
    reset();

    if (verbose)
        printf("%d channel(s) of double array size of %d element(s) (0==scalar), %d iteration(s) per run, %d run(s) (0==forever)\n", channels, arraySize, iterations, runs);

    vector<string> channelNames;
    char buf[64];
    for (int i = 0; i < channels; i++)
    {
        if (arraySize > 0)
            sprintf(buf, "testArray%d_%d", arraySize, i);
        else
            sprintf(buf, "test%d", i);
        channelNames.push_back(buf);
    }

    vector<Channel::shared_pointer> channels;
    for (vector<string>::const_iterator i = channelNames.begin();
            i != channelNames.end();
            i++)
    {
        TR1::shared_ptr<ChannelRequesterImpl> channelRequesterImpl(
            new ChannelRequesterImpl()
        );
        Channel::shared_pointer channel = provider->createChannel(*i, channelRequesterImpl);
        channels.push_back(channel);
    }

    bool differentConnectionsWarningIssued = false;
    string theRemoteAddress;
    for (vector<Channel::shared_pointer>::iterator i = channels.begin();
            i != channels.end();
            i++)
    {
        Channel::shared_pointer channel = *i;
        TR1::shared_ptr<ChannelRequesterImpl> channelRequesterImpl =
            TR1::dynamic_pointer_cast<ChannelRequesterImpl>(channel->getChannelRequester());
        if (channelRequesterImpl->waitUntilConnected(5.0))
        {
            string remoteAddress = channel->getRemoteAddress();
            if (theRemoteAddress.empty())
            {
                theRemoteAddress = remoteAddress;
            }
            else if (theRemoteAddress != remoteAddress)
            {
                if (!differentConnectionsWarningIssued)
                {
                    std::cout << "not all channels are hosted by the same connection: " <<
                              theRemoteAddress << " != " << remoteAddress << std::endl;
                    differentConnectionsWarningIssued = true;
                    // we assumes same connection (thread-safety)
                    exit(2);
                }
            }

            TR1::shared_ptr<ChannelMonitorRequesterImpl> getRequesterImpl(
                new ChannelMonitorRequesterImpl(channel->getChannelName())
            );
            Monitor::shared_pointer monitor = channel->createMonitor(getRequesterImpl, pvRequest);

            bool allOK = getRequesterImpl->waitUntilConnected(timeOut);

            if (!allOK)
            {
                std::cout << "[" << channel->getChannelName() << "] failed to get all the monitors" << std::endl;
                exit(1);
            }

            channelMonitorList.push_back(monitor);

        }
        else
        {
            std::cout << "[" << channel->getChannelName() << "] connection timeout" << std::endl;
            exit(1);
        }
    }
    if (verbose)
        std::cout << "all connected" << std::endl;

    {
        Lock guard(waitLoopPtrMutex);
        waitLoopEvent.reset(new Event());
    }
    epicsTimeGetCurrent(&startTime);
    monitor_all();

    waitLoopEvent->wait();
}

int main (int argc, char *argv[])
{
    int opt;                    // getopt() current option
    std::string testFile;

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    // Set stdout to line buffering

    while ((opt = getopt(argc, argv, ":hr:w:i:c:s:l:f:v")) != -1) {
        switch (opt) {
        case 'h':               // Print usage
            usage();
            return 0;
        case 'w':               // Set PVA timeout value
            if((epicsScanDouble(optarg, &timeOut)) != 1)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('cainfo -h' for help.)\n", optarg);
                timeOut = DEFAULT_TIMEOUT;
            }
            break;
        case 'r':               // pvRequest string
            request = optarg;
            break;
        case 'i':               // iterations
            iterations = atoi(optarg);
            break;
        case 'c':               // channels
            channels = atoi(optarg);
            break;
        case 's':               // arraySize
            arraySize = atoi(optarg);
            break;
        case 'l':               // runs
            runs = atoi(optarg);
            break;
        case 'f':               // testFile
            testFile = optarg;
            break;
        case 'v':               // testFile
            verbose = true;
            break;
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('testGetPerformance -h' for help.)\n",
                    optopt);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('testGetPerformance -h' for help.)\n",
                    optopt);
            return 1;
        default :
            usage();
            return 1;
        }
    }

    // typedef enum {logLevelInfo, logLevelDebug, logLevelError, errlogFatal} errlogSevEnum;
    SET_LOG_LEVEL(logLevelError);

    pvRequest = CreateRequest::create()->createRequest(request);
    if (pvRequest.get() == 0) {
        printf("failed to parse request string\n");
        return 1;
    }

    ClientFactory::start();
    provider = ChannelProviderRegistry::clients()->getProvider("pva");

    if (!testFile.empty())
    {
        ifstream ifs(testFile.c_str(), ifstream::in);
        if (ifs.good())
        {
            string line;
            while (true)
            {
                getline(ifs, line);
                if (ifs.good())
                {
                    // ignore lines that starts (no trimming) with '#'
                    if (line.find('#') != 0)
                    {
                        // <c> <s> <i> <l>
                        if (sscanf(line.c_str(), "%d %d %d %d", &channels, &arraySize, &iterations, &runs) == 4)
                        {
                            //printf("%d %d %d %d\n", channels, arraySize, iterations, runs);
                            runTest();

                            // wait a bit for a next test
                            epicsThreadSleep(1.0);
                        }
                        else
                        {
                            fprintf(stderr,
                                    "Failed to parse line '%s', ignoring...\n",
                                    line.c_str());
                        }
                    }
                }
                else
                    break;
            }
        }
        else
        {
            fprintf(stderr,
                    "Failed to open file '%s'\n",
                    testFile.c_str());
            return 2;
        }

        ifs.close();
    }
    else
    {
        // in non-file mode, verbose is true by default
        verbose = true;
        runTest();
    }

    //ClientFactory::stop();

    return 0;
}
