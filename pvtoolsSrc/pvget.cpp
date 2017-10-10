#include <iostream>
#include <vector>
#include <set>
#include <string>
#include <istream>
#include <fstream>
#include <sstream>

#include <stdio.h>

#if !defined(_WIN32)
#include <signal.h>
#define USE_SIGNAL
#endif

#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsExit.h>
#include <epicsGuard.h>

#include <pv/caProvider.h>
#include <pv/pvAccess.h>
#include <epicsThread.h>
#include <pv/logger.h>
#include <pv/lock.h>
#include <pv/event.h>
#include <pv/reftrack.h>

#include "pvutils.cpp"

using namespace std;
namespace TR1 = std::tr1;
using namespace epics::pvData;
using namespace epics::pvAccess;



typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

namespace {

bool debugFlag = false;

string request("field(value)");
string defaultProvider("pva");

enum PrintMode { ValueOnlyMode, StructureMode, TerseMode };
PrintMode mode = ValueOnlyMode;

char fieldSeparator = ' ';

void usage (void)
{
    fprintf (stderr, "\nUsage: pvget [options] <PV name>...\n\n"
             "\noptions:\n"
             "  -h: Help: Print this message\n"
             "  -V: Print version and exit\n"
             "  -r <pv request>:   Request, specifies what fields to return and options, default is '%s'\n"
             "  -w <sec>:          Wait time, specifies timeout, default is 3 seconds for get, inf. for monitor\n"
             "  -t:                Terse mode - print only value, without names\n"
             "  -i:                Do not format standard types (enum_t, time_t, ...)\n"
             "  -m:                Monitor mode\n"
             "  -p <provider>:     Set default provider name, default is '%s'\n"
             "  -v:                Show entire structure\n"
             "  -q:                Quiet mode, print only error messages\n"
             "  -d:                Enable debug output\n"
             "  -F <ofs>:          Use <ofs> as an alternate output field separator\n"
             "  -f <input file>:   Use <input file> as an input that provides a list PV name(s) to be read, use '-' for stdin\n"
             " enum format:\n"
             "  -n: Force enum interpretation of values as numbers (default is enum string)\n"
//    " time format:\n"
//    "  -u: print userTag\n"
             "\nexample: pvget double01\n\n"
             , request.c_str(), defaultProvider.c_str());
}

void printValue(std::string const & channelName, PVStructure::shared_pointer const & pv)
{
    if (mode == ValueOnlyMode)
    {
        PVField::shared_pointer value = pv->getSubField("value");
        if (value.get() == 0)
        {
            std::cerr << "no 'value' field\n";
            pvutil_ostream myos(std::cout.rdbuf());
            myos << channelName << "\n" << *(pv.get()) << "\n\n";
        }
        else
        {
            Type valueType = value->getField()->getType();
            if (valueType != scalar && valueType != scalarArray)
            {
                // switch to structure mode, unless it's T-type
                if (valueType == structure && isTType(TR1::static_pointer_cast<PVStructure>(value)))
                {
                    std::cout << std::setw(30) << std::left << channelName;
                    std::cout << fieldSeparator;
                    formatTType(std::cout, TR1::static_pointer_cast<PVStructure>(value));
                    std::cout << '\n';
                }
                else
                {
                    pvutil_ostream myos(std::cout.rdbuf());
                    myos << channelName << '\n' << *(pv.get()) << "\n\n";
                }
            }
            else
            {
                if (fieldSeparator == ' ' && value->getField()->getType() == scalar)
                    std::cout << std::setw(30) << std::left << channelName;
                else
                    std::cout << channelName;

                std::cout << fieldSeparator;

                terse(std::cout, value) << '\n';
            }
        }
    }
    else if (mode == TerseMode)
        terseStructure(std::cout, pv) << '\n';
    else
    {
        pvutil_ostream myos(std::cout.rdbuf());
        myos << channelName << '\n' << *(pv.get()) << "\n\n";
    }
}

// tracking get and monitor operations in progress

struct Tracker {
    static epicsMutex doneLock;
    static epicsEvent doneEvt;
    typedef std::set<Tracker*> inprog_t;
    static inprog_t inprog;
    static bool abort;

    Tracker()
    {
        Guard G(doneLock);
        inprog.insert(this);
    }
    ~Tracker()
    {
        done();
    }
    void done()
    {
        {
            Guard G(doneLock);
            inprog.erase(this);
        }
        doneEvt.signal();
    }
};

epicsMutex Tracker::doneLock;
epicsEvent Tracker::doneEvt;
Tracker::inprog_t Tracker::inprog;
bool Tracker::abort = false;

#ifdef USE_SIGNAL
void alldone(int num)
{
    (void)num;
    Tracker::abort = true;
    Tracker::doneEvt.signal();
}
#endif

struct ChannelGetRequesterImpl : public ChannelGetRequester, public Tracker
{
    const string m_channelName;
    operation_type::shared_pointer op;

    ChannelGetRequesterImpl(std::string channelName) : m_channelName(channelName) {}
    virtual ~ChannelGetRequesterImpl() {}

    virtual string getRequesterName()    { return "ChannelGetRequesterImpl"; }

    virtual void channelGetConnect(const epics::pvData::Status& status, ChannelGet::shared_pointer const & channelGet,
                                   epics::pvData::Structure::const_shared_pointer const & /*structure*/)
    {
        if (status.isSuccess())
        {
            if (!status.isOK() || debugFlag)
            {
                std::cerr << "[" << m_channelName << "] channel get create: " << status << '\n';
            }

            channelGet->lastRequest();
            channelGet->get();
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to create channel get: " << status << '\n';
            done();
        }
    }

    virtual void getDone(const epics::pvData::Status& status,
                         ChannelGet::shared_pointer const & /*channelGet*/,
                         epics::pvData::PVStructure::shared_pointer const & pvStructure,
                         epics::pvData::BitSet::shared_pointer const & bitSet)
    {
        if (status.isSuccess())
        {
            if (!status.isOK() || debugFlag)
            {
                std::cerr << "[" << m_channelName << "] channel get: " << status << '\n';
            }

            printValue(m_channelName, pvStructure);

        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to get: " << status << '\n';
        }

        done();
    }

};

struct MonitorRequesterImpl : public MonitorRequester, public Tracker
{

    const string m_channelName;
    operation_type::shared_pointer op;

    MonitorRequesterImpl(std::string channelName) : m_channelName(channelName) {}
    virtual ~MonitorRequesterImpl() {}

    virtual string getRequesterName()
    {
        return "MonitorRequesterImpl";
    }

    virtual void monitorConnect(const epics::pvData::Status& status, Monitor::shared_pointer const & monitor, StructureConstPtr const & /*structure*/)
    {
        if (status.isSuccess())
        {
            Status startStatus = monitor->start();
            // show error
            // TODO and exit
            if (!startStatus.isSuccess() || debugFlag)
            {
                std::cerr << "[" << m_channelName << "] channel monitor start: " << startStatus << '\n';
            }

        }
        else
        {
            std::cerr << "monitorConnect(" << status << ")\n";
            done();
        }
    }

    virtual void channelDisconnect(bool destroy) {
        if(!destroy) {
            std::cerr << m_channelName<<" Disconnected\n";
        }
    }

    virtual void monitorEvent(Monitor::shared_pointer const & monitor)
    {
        if(debugFlag)
            std::cerr << "[" << m_channelName << "] channel monitor event: \n";

        for(MonitorElement::Ref it(monitor); it; ++it)
        {
            MonitorElement* element(it.get());

            if (mode == ValueOnlyMode)
            {
                PVField::shared_pointer value = element->pvStructurePtr->getSubField("value");
                if (value.get() == 0)
                {
                    std::cerr << "no 'value' field" << '\n';
                    std::cout << m_channelName << '\n';
                    pvutil_ostream myos(std::cout.rdbuf());
                    myos << *(element->pvStructurePtr.get()) << "\n\n";
                }
                else
                {
                    Type valueType = value->getField()->getType();
                    if (valueType != scalar && valueType != scalarArray)
                    {
                        // switch to structure mode, unless it's T-type
                        if (valueType == structure && isTType(TR1::static_pointer_cast<PVStructure>(value)))
                        {
                            std::cout << std::setw(30) << std::left << m_channelName;
                            std::cout << fieldSeparator;
                            formatTType(std::cout, TR1::static_pointer_cast<PVStructure>(value));
                            std::cout << '\n';
                        }
                        else
                        {
                            std::cout << m_channelName << '\n';
                            pvutil_ostream myos(std::cout.rdbuf());
                            myos << *(element->pvStructurePtr.get()) << "\n\n";
                        }
                    }
                    else
                    {
                        if (fieldSeparator == ' ' && value->getField()->getType() == scalar)
                            std::cout << std::setw(30) << std::left << m_channelName;
                        else
                            std::cout << m_channelName;

                        std::cout << fieldSeparator;

                        terse(std::cout, value) << '\n';
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

                terseStructure(std::cout, element->pvStructurePtr) << '\n';
            }
            else
            {
                std::cout << m_channelName << '\n';
                pvutil_ostream myos(std::cout.rdbuf());
                myos << *(element->pvStructurePtr.get()) << "\n\n";
            }

        }

    }

    virtual void unlisten(Monitor::shared_pointer const & /*monitor*/)
    {
        if(debugFlag)
            std::cerr << "unlisten" << m_channelName << '\n';
        done();
    }
};

} // namespace


int main (int argc, char *argv[])
{
    int opt;                    /* getopt() current option */
    bool monitor = false;

    istream* inputStream = 0;
    ifstream ifs;
    bool fromStream = false;
    epics::RefMonitor refmon;

    double timeOut = -1.0;
    bool explicit_timeout = false;

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */

    // ================ Parse Arguments

    while ((opt = getopt(argc, argv, ":hvVRr:w:tmp:qdcF:f:ni")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'v':
            mode = StructureMode;
            break;
        case 'V':               /* Print version */
        {
            Version version("pvget", "cpp",
                    EPICS_PVA_MAJOR_VERSION,
                    EPICS_PVA_MINOR_VERSION,
                    EPICS_PVA_MAINTENANCE_VERSION,
                    EPICS_PVA_DEVELOPMENT_FLAG);
            fprintf(stdout, "%s\n", version.getVersionString().c_str());
            return 0;
        }
        case 'R':
            refmon.start(5.0);
            break;
        case 'w':               /* Set PVA timeout value */
            if((epicsScanDouble(optarg, &timeOut)) != 1 || timeOut <= 0.0)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('pvget -h' for help.)\n", optarg);
            } else {
                explicit_timeout = true;
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
        case 'p':               /* Set default provider */
            defaultProvider = optarg;
            break;
        case 'q':               /* Quiet mode */
            break;
        case 'd':               /* Debug log level */
            debugFlag = true;
            break;
        case 'c':               /* Clean-up and report used instance count */
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

    if(!explicit_timeout) {
        if(monitor)
            timeOut = -1.0; // forever
        else
            timeOut = 3.0;
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


    SET_LOG_LEVEL(debugFlag ? logLevelDebug : logLevelError);

    std::cout << std::boolalpha;
    terseSeparator(fieldSeparator);

    // ================ Connect channels and start operations

    epics::pvAccess::ca::CAClientFactory::start();

    bool allOK = true;

    std::set<ChannelProvider::shared_pointer> providers;
    typedef std::map<std::string, Channel::shared_pointer> chan_cache_t;
    chan_cache_t chan_cache;

    PVStructure::shared_pointer pvRequest;
    try {
        pvRequest = createRequest(request);
    } catch(std::exception& e){
        fprintf(stderr, "failed to parse request string: %s\n", e.what());
        return 1;
    }

    // keep the operations, and associated channels, alive
    std::vector<std::tr1::shared_ptr<Tracker> > ops;

    for(size_t n=0; n<pvs.size(); n++)
    {
        URI uri;
        bool validURI = URI::parse(pvs[n], uri);

        if (validURI) {
            if (uri.path.length() <= 1)
            {
                std::cerr << "invalid URI '" << pvs[n] << "', empty path\n";
                return 1;
            }
            pvs[n] = uri.path.substr(1);;
        } else {
            uri.protocol = defaultProvider;
            uri.host.clear();
        }

        ChannelProvider::shared_pointer provider(ChannelProviderRegistry::clients()->getProvider(uri.protocol));
        if(!provider) {
            std::cerr<<"Unknown provider \""<<uri.protocol<<"\" for channel "<<pvs[n]<<"\n";
            return 1;
        }

        Channel::shared_pointer channel;
        chan_cache_t::const_iterator it = chan_cache.find(pvs[n]);
        if(it==chan_cache.end()) {
            try {
                channel = provider->createChannel(pvs[n], DefaultChannelRequester::build(),
                                                  ChannelProvider::PRIORITY_DEFAULT, uri.host);
            } catch(std::exception& e) {
                std::cerr<<"Provider "<<uri.protocol<<" can't create channel \""<<pvs[n]<<"\"\n";
                return 1;
            }
            chan_cache[pvs[n]] = channel;
        } else {
            channel = it->second;
        }

        if(monitor) {
            std::tr1::shared_ptr<MonitorRequesterImpl> req(new MonitorRequesterImpl(pvs[n]));

            req->op = channel->createMonitor(req, pvRequest);

            ops.push_back(req);

        } else {
            std::tr1::shared_ptr<ChannelGetRequesterImpl> req(new ChannelGetRequesterImpl(pvs[n]));

            req->op = channel->createChannelGet(req, pvRequest);

            ops.push_back(req);
        }

        // make sure to keep the provider alive as Channels will be automatically closed
        providers.insert(provider);
    }

    // Active channels continue to be referenced by get/monitor stored in 'ops'
    chan_cache.clear();

    // ========================== Wait for operations to complete, or timeout

#ifdef USE_SIGNAL
        signal(SIGINT, alldone);
        signal(SIGTERM, alldone);
        signal(SIGQUIT, alldone);
#endif

    if(debugFlag)
        std::cerr<<"Waiting...\n";

    {
        Guard G(Tracker::doneLock);
        while(Tracker::inprog.size() && !Tracker::abort) {
            UnGuard U(G);
            if(timeOut<=0)
                Tracker::doneEvt.wait();
            else if(!Tracker::doneEvt.wait(timeOut)) {
                allOK = false;
                if(debugFlag)
                    std::cerr<<"Timeout\n";
                break;
            }
        }
    }

    if(refmon.running()) {
        refmon.stop();
        // drop refs to operations, but keep ref to ClientProvider
        ops.clear();
        // show final counts
        refmon.current();
    }

    // ========================== All done now

    if(debugFlag)
        std::cerr<<"Done\n";
    return allOK ? 0 : 1;
}
