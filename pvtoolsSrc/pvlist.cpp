/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <stdio.h>

#include <iostream>
#include <map>
#include <iterator>
#include <vector>
#include <string>
#include <istream>
#include <fstream>
#include <sstream>


#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <pv/logger.h>

#include <epicsExit.h>

#include <pv/inetAddressUtil.h>
#include <pv/configuration.h>
#include <pv/remote.h>

#include "pvutils.h"

#if defined(_WIN32) && !defined(_MINGW)
FILE *popen(const char *command, const char *mode) {
    return _popen(command, mode);
}
int pclose(FILE *stream) {
    return _pclose(stream);
}
#endif

using namespace std;

using namespace epics::pvData;
using namespace epics::pvAccess;

namespace {

#define DEFAULT_TIMEOUT 3.0

void usage (void)
{
    fprintf (stderr, "\nUsage: pvlist [options] [<server address or GUID starting with '0x'>]...\n\n"
             "\noptions:\n"
             "  -h: Help: Print this message\n"
             "  -V: Print version and exit\n"
             "  -i                 Print server info (when server address list/GUID is given)\n"
             "  -w <sec>:          Wait time, specifies timeout, default is %f second(s)\n"
             "  -q:                Quiet mode, print only error messages\n"
             "  -d:                Enable debug output\n"
//    "  -F <ofs>:          Use <ofs> as an alternate output field separator\n"
//    "  -f <input file>:   Use <input file> as an input that provides a list input parameters(s) to be read, use '-' for stdin\n"
             "\nexamples:\n"
             "\tpvlist\n"
             "\tpvlist ioc0001\n"
             "\tpvlist 10.5.1.205:10000\n"
             "\tpvlist 0x83DE3C540000000000BF351F\n\n"
             , DEFAULT_TIMEOUT);
}

}//namespace

/*+**************************************************************************
 *
 * Function:    main
 *
 * Description: pvlist main()
 *              Evaluate command line options, ...
 *
 * Arg(s) In:   [options] [<server>]...
 *
 * Arg(s) Out:  none
 *
 * Return(s):   Standard return code (0=success, 1=error)
 *
 **************************************************************************-*/

int main (int argc, char *argv[])
{
    int opt;                    /* getopt() current option */
    bool debug = false;
    double timeOut = DEFAULT_TIMEOUT;
    bool printInfo = false;

    /*
    istream* inputStream = 0;
    ifstream ifs;
    bool fromStream = false;
    */
    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */

    while ((opt = getopt(argc, argv, ":hVw:qdF:f:i")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'V':               /* Print version */
        {
            fprintf(stdout, "pvAccess %u.%u.%u%s\n",
                    EPICS_PVA_MAJOR_VERSION,
                    EPICS_PVA_MINOR_VERSION,
                    EPICS_PVA_MAINTENANCE_VERSION,
                    (EPICS_PVA_DEVELOPMENT_FLAG)?"-SNAPSHOT":"");
            fprintf(stdout, "pvData %u.%u.%u%s\n",
                    EPICS_PVD_MAJOR_VERSION,
                    EPICS_PVD_MINOR_VERSION,
                    EPICS_PVD_MAINTENANCE_VERSION,
                    (EPICS_PVD_DEVELOPMENT_FLAG)?"-SNAPSHOT":"");
            fprintf(stdout, "Base %s\n", EPICS_VERSION_FULL);
            return 0;
        }
        case 'w':               /* Set PVA timeout value */
            if((epicsScanDouble(optarg, &timeOut)) != 1 || timeOut <= 0.0)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('pvlist -h' for help.)\n", optarg);
                timeOut = DEFAULT_TIMEOUT;
            }
            break;
        case 'q':               /* Quiet mode */
            break;
        case 'd':               /* Debug log level */
            debug = true;
            break;
        case 'i':               /* Print server info */
            printInfo = true;
            break;
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('pvlist -h' for help.)\n",
                    optopt);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('pvlist -h' for help.)\n",
                    optopt);
            return 1;
        default :
            usage();
            return 1;
        }
    }

    SET_LOG_LEVEL(debug ? logLevelDebug : logLevelError);

    bool noArgs = (optind == argc);

    bool byGUIDSearch = false;
    for (int i = optind; i < argc; i++)
    {
        string serverAddress = argv[i];

        // by GUID search
        if (serverAddress.length() == 26 &&
                serverAddress[0] == '0' &&
                serverAddress[1] == 'x')
        {
            byGUIDSearch = true;
            break;
        }
    }

    bool allOK = true;

    ServerMap serverMap;
    if (noArgs || byGUIDSearch)
        discoverServers(timeOut, serverMap);

    // just list all the discovered servers
    if (noArgs)
    {
        for (ServerMap::const_iterator iter = serverMap.begin();
                iter != serverMap.end();
                iter++)
        {
            const ServerEntry& entry = iter->second;

            cout << "GUID 0x" << entry.guid << " version " << (int)entry.version << ": "
                 << entry.protocol << "@[ ";

            size_t count = entry.addresses.size();
            for (size_t i = 0; i < count; i++)
            {
                cout << inetAddressToString(entry.addresses[i]);
                if (i < (count-1))
                    cout << " ";
            }
            cout << " ]" << endl;
        }
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            string serverAddress = argv[i];

            // by GUID search
            if (serverAddress.length() == 26 &&
                    serverAddress[0] == '0' &&
                    serverAddress[1] == 'x')
            {
                bool resolved = false;
                for (ServerMap::const_iterator iter = serverMap.begin();
                        iter != serverMap.end();
                        iter++)
                {
                    const ServerEntry& entry = iter->second;

                    if (strncmp(entry.guid.c_str(), &(serverAddress[2]), 24) == 0)
                    {
                        // found match

                        // TODO for now we take only first server address
                        serverAddress = inetAddressToString(entry.addresses[0]);
                        resolved = true;
                        break;
                    }
                }

                if (!resolved)
                {
                    fprintf(stderr, "Failed to resolve GUID '%s'!\n", serverAddress.c_str());
                    allOK = false;
                    continue;
                }
            }

            try
            {
                PVStructure::shared_pointer ret = getChannelInfo(serverAddress, printInfo ? "info" : "channels", timeOut);

                if(!printInfo)
                {
                    PVStringArray::shared_pointer pvs(ret->getSubField<PVStringArray>("value"));

                    PVStringArray::const_svector val(pvs->view());

                    std::copy(val.begin(), val.end(), std::ostream_iterator<std::string>(std::cout, "\n"));
                }
                else {
                    std::cout<<ret<<"\n";
                }
            }
            catch(std::exception& e)
            {
                std::cerr<<"Error: "<<e.what()<<"\n";
                return 1;
            }
        }
    }

    return allOK ? 0 : 1;
}
