/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

/*
 * PVA Name Server utility. It polls a set of PVA
 * servers for a list of channels, and resolves channel queries.
 * PVA servers can be discovered, they can be passed through the
 * command line, or they can be specified via an input file which
 * gets re-read at runtime.
 */

#include <stdio.h>

#include <iostream>
#include <string>
#include <istream>
#include <fstream>
#include <sstream>
#include <vector>

#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsExit.h>

#include <pv/logger.h>
#include <pv/configuration.h>
#include <pv/stringUtility.h>

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
        "  -f <input file>\t:\tStatic server list file with '<HOST:PORT>' entries\n"
        "  -F <input file>\t:\tStatic channel map file with '<CHANNEL> <HOST:PORT>' entries\n"
        "  -s <addr>,<addr>,...\t:\tComma-separated list of '<HOST:PORT>' static server entries\n"
        "  -a\t\t\t:\tAuto mode, discover severs available on the network\n"
        "  -p <poll period>\t:\tServer poll period in seconds (default: %.2f [s])\n"
        "  -w <wait period>\t:\tServer wait time in seconds (default: %.2f [s])\n"
        "  -e <expiration time>\t:\tChannel entry expiration time in seconds (default: %.2f [s])\n"
        "  -d\t\t\t:\tEnable debug output\n"
        "\nDifferent inputs for PVA server address will be combined."
        "\nServer list file should contain '<HOST:PORT>' entries separated by spaces or commas, or on different lines."
        "\nChannel map file should contain '<CHANNEL> <HOST:PORT>' entries separated by spaces or commas, or on different lines."
        "\nChannel expiration time <= 0 indicates that channel entries"
        "\nnever expire.\n\n"
        , DEFAULT_POLL_PERIOD, DEFAULT_PVA_TIMEOUT, DEFAULT_CHANNEL_EXPIRATION_TIME);
}

// Expected server address format: <HOST:PORT>
// There can be multiple addresses per line, separated by spaces or commas. 
std::string readServerAddressesFromFile(const std::string& inputFile, const std::string& existingAddresses = "")
{
    std::string serverAddresses = existingAddresses;
    if (inputFile.empty()) {
        return serverAddresses;
    }
    std::ifstream ifs(inputFile);
    std::string line;
    while (std::getline(ifs, line)) {
        line = StringUtility::replace(line, ',', " ");
        serverAddresses = serverAddresses + " " + line;
    }
    return serverAddresses;
}

// Expected channel entry format: <CHANNEL_NAME> <HOST:PORT>
// There can be multiple entries per line, separated by spaces or commas. 
void readChannelAddressesFromFile(const std::string& inputFile, ChannelMap& channelMap)
{
    if (inputFile.empty()) {
        return;
    }
    bool ignoreEmptyTokens = true;
    epicsTimeStamp now;
    epicsTimeGetCurrent(&now);
    std::ifstream ifs(inputFile);
    std::string line;
    while (std::getline(ifs, line)) {
        line = StringUtility::replace(line, ',', " ");
        std::vector<std::string> tokens = StringUtility::split(line, ' ', ignoreEmptyTokens);
        int nTokens = int(tokens.size());
        for (int i = 0; i < nTokens-1; i+=2) {
            std::string channelName = tokens[i];
            std::string serverAddress = tokens[i+1];
            ChannelEntry channelEntry = {channelName, serverAddress, now};
            channelMap[channelName] = channelEntry;
            LOG(logLevelDebug, "Adding %s/%s channel entry", channelName.c_str(), serverAddress.c_str()); 
        }
    }
}

} // namespace

int main(int argc, char *argv[])
{
    int opt;                    /* getopt() current option */
    bool debug = false;
    bool autoDiscovery = false;
    double timeout = DEFAULT_PVA_TIMEOUT;
    double pollPeriod = DEFAULT_POLL_PERIOD;
    double channelExpirationTime = DEFAULT_CHANNEL_EXPIRATION_TIME;
    std::string serverAddresses;
    std::string serverListFile;
    std::string channelMapFile;

    while ((opt = getopt(argc, argv, ":hHVw:e:p:das:f:F:")) != -1) {
        switch (opt) {
            case 'h':               /* Print usage */
            case 'H': {             /* Print usage */
                usage();
                return 0;
            }
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
                serverListFile = optarg;
                break;
            }
            case 'F': {             /* Channel map file */
                channelMapFile = optarg;
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
        // Reread input file before polling.
        std::string staticServerAddresses = readServerAddressesFromFile(serverListFile, serverAddresses);
        srv->setStaticServerAddresses(staticServerAddresses);
        ChannelMap staticChannelMap;
        readChannelAddressesFromFile(channelMapFile, staticChannelMap);
        srv->setStaticChannelEntries(staticChannelMap);
        srv->run(pollPeriod);
    }
    return 0;
}
