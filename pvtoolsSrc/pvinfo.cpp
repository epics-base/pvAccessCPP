#include <iostream>
#include <pv/pvAccess.h>
#include <pv/caProvider.h>

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

using namespace std;
namespace TR1 = std::tr1;
using namespace epics::pvData;
using namespace epics::pvAccess;


#define DEFAULT_TIMEOUT 3.0
#define DEFAULT_PROVIDER "pva"

double timeOut = DEFAULT_TIMEOUT;
string defaultProvider(DEFAULT_PROVIDER);
const string noAddress;

void usage (void)
{
    fprintf (stderr, "\nUsage: pvinfo [options] <PV name>...\n\n"
             "\noptions:\n"
             "  -h: Help: Print this message\n"
             "  -V: Print version and exit\n"
             "  -w <sec>:          Wait time, specifies timeout, default is %f second(s)\n"
             "  -p <provider>:     Set default provider name, default is '%s'\n"
             "  -d:                Enable debug output\n"
             "  -c:                Wait for clean shutdown and report used instance count (for expert users)"
             "\nExample: pvinfo double01\n\n"
             , DEFAULT_TIMEOUT, DEFAULT_PROVIDER);
}


/*+**************************************************************************
 *
 * Function:	main
 *
 * Description:	pvinfo main()
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

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */

    while ((opt = getopt(argc, argv, ":hVw:p:dc")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'V':               /* Print version */
        {
            Version version("pvinfo", "cpp",
                    EPICS_PVA_MAJOR_VERSION,
                    EPICS_PVA_MINOR_VERSION,
                    EPICS_PVA_MAINTENANCE_VERSION,
                    EPICS_PVA_DEVELOPMENT_FLAG);
            fprintf(stdout, "%s\n", version.getVersionString().c_str());
            return 0;
        }
        case 'w':               /* Set PVA timeout value */
            if((epicsScanDouble(optarg, &timeOut)) != 1 || timeOut <= 0.0)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('pvget -h' for help.)\n", optarg);
                timeOut = DEFAULT_TIMEOUT;
            }
            break;
        case 'p':               /* Set default provider */
            defaultProvider = optarg;
            break;
        case 'd':               /* Debug log level */
            debug = true;
            break;
        case 'c':               /* Clean-up and report used instance count */
            cleanupAndReport = true;
            break;
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('pvinfo -h' for help.)\n",
                    optopt);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('pvinfo -h' for help.)\n",
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
        fprintf(stderr, "No pv name(s) specified. ('pvinfo -h' for help.)\n");
        return 1;
    }

    vector<string> pvs;     /* Array of PV names */
    for (int n = 0; optind < argc; n++, optind++)
        pvs.push_back(argv[optind]);       /* Copy PV names from command line */


    SET_LOG_LEVEL(debug ? logLevelDebug : logLevelError);

    std::cout << std::boolalpha;

    bool allOK = true;

    epics::pvAccess::ca::CAClientFactory::start();

    {
        std::vector<std::string> pvNames;
        std::vector<std::string> pvAddresses;
        std::vector<ChannelProvider::shared_pointer> providers;

        pvNames.reserve(nPvs);
        pvAddresses.reserve(nPvs);

        for (int n = 0; n < nPvs; n++)
        {
            URI uri;
            bool validURI = URI::parse(pvs[n], uri);

            std::string providerName(defaultProvider);
            std::string pvName(pvs[n]);
            std::string address(noAddress);
            if (validURI)
            {
                if (uri.path.length() <= 1)
                {
                    std::cerr << "invalid URI '" << pvs[n] << "', empty path" << std::endl;
                    return 1;
                }
                providerName = uri.protocol;
                pvName = uri.path.substr(1);
                address = uri.host;
            }

            pvNames.push_back(pvName);
            pvAddresses.push_back(address);
            providers.push_back(ChannelProviderRegistry::clients()->getProvider(providerName));
            if(!providers.back())
            {
                std::cerr << "unknown provider name '" << providerName
                          << "', only 'pva' and 'ca' are supported" << std::endl;
                allOK = false;
            }
        }

        // first connect to all, this allows resource (e.g. TCP connection) sharing
        vector<Channel::shared_pointer> channels(nPvs);
        for (int n = 0; n < nPvs; n++)
        {
            if(!providers[n])
			{
				std::cerr << "Invalid provider for " << pvs[n] << std::endl;
				continue;
			}
            channels[n] = providers[n]->createChannel(pvNames[n], DefaultChannelRequester::build(),
                                                      ChannelProvider::PRIORITY_DEFAULT, pvAddresses[n]);
			if ( !channels[n] )
			{
				std::cerr << "Unable to create channel for '" << pvs[n] << std::endl;
			}
        }

        // for now a simple iterating sync implementation, guarantees order
        for (int n = 0; n < nPvs; n++)
        {
            Channel::shared_pointer channel = channels[n];
			if ( !channel )
				continue;

            TR1::shared_ptr<GetFieldRequesterImpl> getFieldRequesterImpl(new GetFieldRequesterImpl(channel));
            channel->getField(getFieldRequesterImpl, "");

            if (getFieldRequesterImpl->waitUntilFieldGet(timeOut))
            {
                Structure::const_shared_pointer structure =
                    TR1::dynamic_pointer_cast<const Structure>(getFieldRequesterImpl->getField());

                channel->printInfo();
                if (structure)
                {
                    std::cout << *structure << std::endl << std::endl;
                }
                else
                {
                    std::cout << "(null introspection data)" << std::endl << std::endl;
                }
            }
            else
            {
                allOK = false;
                std::cerr << "[" << channel->getChannelName() << "] failed to get channel introspection data" << std::endl;
            }
        }

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
