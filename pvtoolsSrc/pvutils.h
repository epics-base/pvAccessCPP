/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
#ifndef PVUTILS_H
#define PVUTILS_H

#include <ostream>
#include <iostream>
#include <string>
#include <vector>
#include <map>

#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsGuard.h>
#include <osiSock.h>

#include <pv/event.h>
#include <pv/pvData.h>
#include <pv/pvAccess.h>
#include <pv/pvaConstants.h>

typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;
namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

#define COMMON_OPTIONS \
    "options:\n" \
    "  -h: Help: Print this message\n" \
    "  -V: Print version and exit\n" \
    "  -r <pv request>:   Request, specifies what fields to return and options, default is '%s'\n" \
    "  -w <sec>:          Wait time, specifies timeout, default is %f second(s)\n" \
    "  -p <provider>:     Set default provider name, default is '%s'\n" \
    "  -M <raw|nt|json>:  Output mode.  default is 'nt'\n" \
    "  -v:                Show entire structure (implies Raw mode)\n" \
    "  -q:                Quiet mode, print only error messages\n" \
    "  -d:                Enable debug output\n"

extern double timeout;
extern bool debugFlag;

extern pvd::PVStructure::Formatter::format_t outmode;
extern int verbosity;

extern std::string request;
extern std::string defaultProvider;

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

    static void prepare();
    EPICS_NOT_COPYABLE(Tracker)
};

struct ServerEntry {
    std::string guid;
    std::string protocol;
    std::vector<osiSockAddr> addresses;
    pvd::int8 version;
};
typedef std::map<std::string, ServerEntry> ServerMap;

void jarray(pvd::shared_vector<std::string>& out, const char *inp);
std::string toHex(pvd::int8* ba, size_t len);
std::size_t readSize(pvd::ByteBuffer* buffer);
std::string deserializeString(pvd::ByteBuffer* buffer);
bool processSearchResponse(const osiSockAddr& responseFrom, pvd::ByteBuffer& receiveBuffer, ServerMap& serverMapByGuid);
bool discoverServers(double timeOut, ServerMap& serverMapByGuid);
pvd::PVStructure::shared_pointer getChannelInfo(const std::string& serverAddress, const std::string& queryOp, double timeOut);


#endif /* PVUTILS_H */
