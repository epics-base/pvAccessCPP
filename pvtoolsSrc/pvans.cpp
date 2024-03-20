/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <stdio.h>

#include <iostream>
#include <string>
#include <istream>
#include <fstream>
#include <sstream>

#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsExit.h>

#include <pv/logger.h>
#include <pv/configuration.h>

#include "nameServer.h"
#include "pvutils.h"

using namespace std;

using namespace epics::pvData;
using namespace epics::pvAccess;

namespace {

#define DEFAULT_PVA_TIMEOUT 3.0
#define DEFAULT_POLL_PERIOD 900.0
#define DEFAULT_CHANNEL_EXPIRATION_TIME 2*DEFAULT_POLL_PERIOD

void usage (void) {
    fprintf (stderr,
        "\nPVA Name Server\n"
        "\nUsage: pvans [options]...\n\n"
        "\noptions:\n"
        "  -h|-H\t\t\t:\tHelp: Print this message\n"
        "  -V\t\t\t:\tPrint version and exit\n"
        "  -f <input file>\t:\tInput file containing list of PVA server addresses in the form <HOST>:<PORT>\n"
        "  -s <addr>,<addr>,...\t:\tComma-separated list of PVA server addresses in the form <HOST>:<PORT>\n"
        "  -a\t\t\t:\tAuto mode, discover severs available on the network\n"
        "  -p <poll period>\t:\tServer poll period in seconds (default: %.2f [s])\n"
        "  -w <wait period>\t:\tServer wait time in seconds (default: %.2f [s])\n"
        "  -e <expiration time>\t:\tChannel entry expiration time in seconds (default: %.2f [s])\n"
        "  -d\t\t\t:\tEnable debug output\n"
        "\nDifferent inputs for PVA server address will be combined."
        "\nChannel expiration time <= 0 indicates that channel entries never expire.\n\n"
        , DEFAULT_POLL_PERIOD, DEFAULT_PVA_TIMEOUT, DEFAULT_CHANNEL_EXPIRATION_TIME);
}

std::string addServerAddressesFromFile(const std::string& inputFile, const std::string& existingAddresses = "")
{
    std::string serverAddresses = existingAddresses;
    if (inputFile.empty()) {
        return serverAddresses;
    }
    std::ifstream ifs(inputFile);
    std::string line;
    while (std::getline(ifs, line)) {
        serverAddresses = serverAddresses + " " + line;
    }
    return serverAddresses;
}

} //namespace

int main(int argc, char *argv[])
{
    int opt;                    /* getopt() current option */
    bool debug = false;
    bool autoDiscovery = false;
    double timeout = DEFAULT_PVA_TIMEOUT;
    double pollPeriod = DEFAULT_POLL_PERIOD;
    double channelExpirationTime = DEFAULT_CHANNEL_EXPIRATION_TIME;
    std::string inputFile;
    std::string serverAddresses;

    while ((opt = getopt(argc, argv, ":hHVw:e:p:das:f:")) != -1) {
        switch (opt) {
            case 'h':               /* Print usage */
            case 'H':               /* Print usage */
                usage();
                return 0;
            case 'V': {             /* Print version */
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
            case 'p': {             /* Set poll period */
                if((epicsScanDouble(optarg, &pollPeriod)) != 1 || pollPeriod <= 0.0) {
                    fprintf(stderr,
                        "'%s' is not a valid poll period value "
                        "- ignored. ('pvans -h' for help.)\n", optarg);
                    timeout = DEFAULT_PVA_TIMEOUT;
                }
                break;
            }
            case 'w': {             /* Set PVA timeout value */
                if((epicsScanDouble(optarg, &timeout)) != 1 || timeout <= 0.0) {
                    fprintf(stderr,
                        "'%s' is not a valid timeout value "
                        "- ignored. ('pvans -h' for help.)\n", optarg);
                    timeout = DEFAULT_PVA_TIMEOUT;
                }
                break;
            }
            case 'e': {             /* Set channel expiration time */
                if((epicsScanDouble(optarg, &channelExpirationTime)) != 1) {
                    fprintf(stderr,
                        "'%s' is not a valid expiration time value "
                        "- ignored. ('pvans -h' for help.)\n", optarg);
                    channelExpirationTime = DEFAULT_CHANNEL_EXPIRATION_TIME;
                }
                break;
            }
            case 's': {             /* Server list */
                serverAddresses = optarg;
                break;
            }
            case 'f': {             /* Server list file */
                inputFile = optarg;
                break;
            }
            case 'd': {             /* Debug log level */
                debug = true;
                break;
            }
            case 'a': {             /* Auto discovery */
                autoDiscovery = true;
                break;
            }
            case '?': {
                fprintf(stderr,
                    "Unrecognized option: '-%c'. ('pvans  -h' for help.)\n",
                    optopt);
                return 1;
            }
            case ':': {
                fprintf(stderr,
                    "Option '-%c' requires an argument. ('pvans -h' for help.)\n",
                    optopt);
                return 1;
            }
            default: {
                usage();
                return 1;
            }
        }
    }

    SET_LOG_LEVEL(debug ? logLevelDebug : logLevelError);

    NameServer::shared_pointer srv(new NameServer(ConfigurationBuilder()
                                   .push_env()
                                   .push_map()
                                   .build()));
    srv->setPollPeriod(pollPeriod);
    srv->setPvaTimeout(timeout);
    srv->setAutoDiscovery(autoDiscovery);
    srv->setChannelEntryExpirationTime(channelExpirationTime);
    while (true) {
        std::string allServerAddresses = addServerAddressesFromFile(inputFile, serverAddresses);
        srv->setServerAddresses(allServerAddresses);
        srv->run(pollPeriod);
    }
    return 0;
}
