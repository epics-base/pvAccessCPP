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
};

void jarray(pvd::shared_vector<std::string>& out, const char *inp);


#endif /* PVUTILS_H */
