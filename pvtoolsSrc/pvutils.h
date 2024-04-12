/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
#ifndef PVUTILS_H
#define PVUTILS_H

#include <ostream>
#include <iostream>
#include <string>

#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsGuard.h>

#include <pv/event.h>
#include <pv/pvData.h>
#include <pv/pvAccess.h>

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

void jarray(pvd::shared_vector<std::string>& out, const char *inp);


#endif /* PVUTILS_H */
