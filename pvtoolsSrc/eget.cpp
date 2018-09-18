/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <string.h>

#define EXECNAME "eget"

#define MAIN getMain
#include "pvget.cpp"

#undef MAIN
#define MAIN callMain
#include "pvcall.cpp"

static
void egetUsage (void)
{
    fprintf (stderr, "\nUsage: eget [options] [-s] <PV name>... [-a <argname>=<argvalue>]...\n\n"
             "\noptions:\n"
             "  -h: Help: Print this message\n"
             "  -V: Print version and exit\n"
             "  -s <service name>:   Service API compliant based RPC service name (accepts NTURI request argument)\n"
             "  -a <service arg>:    Service argument in 'name[=value]' or 'name value' form\n"
             "  -r <pv request>:     Get request string, specifies what fields to return and options, default is '%s'\n"
             "  -w <sec>:            Wait time, specifies timeout, default is %f second(s)\n"
//             "  -z:                  Pure pvAccess RPC based service (send NTURI.query as request argument)\n"
//             "  -N:                  Do not format NT types, dump structure instead\n"
//             "  -i:                  Do not format standard types (enum_t, time_t, ...)\n"
//             "  -t:                  Terse mode\n"
//             "  -T:                  Transpose vector, table, matrix\n"
//             "  -x:                  Use column-major order to decode matrix\n"
             "  -p <provider>:       Set default provider name, default is '%s'\n"
//             "  -q:                  Quiet mode, print only error messages\n"
             "  -d:                  Enable debug output\n"
//             "  -F <ofs>:            Use <ofs> as an alternate output field fieldSeparator\n"
//             "  -f <input file>:     Use <input file> as an input that provides a list PV name(s) to be read, use '-' for stdin\n"
//             "  -c:                  Wait for clean shutdown and report used instance count (for expert users)\n"
//             " enum format:\n"
//             "  -n: Force enum interpretation of values as numbers (default is enum string)\n"
             "\n\nexamples:\n\n"
             "#! Get the value of the PV corr:li32:53:bdes\n"
             "> eget corr:li32:53:bdes\n"
             "\n"
             "#! Get the table of all correctors from the rdb service\n"
             "> eget -s rdbService -a entity=swissfel:devicenames\n"
             "\n"
             "#! Get the archive history of quad45:bdes;history between 2 times, from the archive service\n"
             "> eget -s archiveService -a entity=quad45:bdes;history -a starttime=2012-02-12T10:04:56 -a endtime=2012-02-01T10:04:56\n"
             "\n"
             "#! Get polynomials for bunch of quads using a stdin to give a list of PV names\n"
             "> eget -s names -a pattern=QUAD:LTU1:8%%:POLYCOEF | eget -f -\n"
             "\n"
             , request.c_str(), timeout, defaultProvider.c_str());
}

int main (int argc, char *argv[])
{
    bool found_s = false;
    for(int i=1; i<argc && !found_s; i++) {
        if(argv[i][0]!='-') continue;
        switch(argv[i][1]) {
        case 's': found_s = true; break;
        case 'h': egetUsage(); return 1;
        }
    }
    if(found_s)
        return callMain(argc, argv);
    else
        return getMain(argc, argv);
}
