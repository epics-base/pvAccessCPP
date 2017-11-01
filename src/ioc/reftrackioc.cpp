/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <exception>

#include <iocsh.h>

#include <pv/reftrack.h>
#include <pv/iocshelper.h>

#include <epicsExport.h>

#include <pv/iocreftrack.h>

namespace {

void showRefs(const epics::RefSnapshot& snap, int lvl, bool delta)
{
    for(epics::RefSnapshot::const_iterator it = snap.begin(), end = snap.end();
        it != end; ++it)
    {
        if(it->second.current==0 && it->second.delta==0 && lvl<=0) continue;
        if(delta && it->second.delta==0 && lvl<=0) continue;
        if(delta) {
            printf(" %s : %zu (delta %zd)\n",
                   it->first.c_str(), it->second.current, it->second.delta);
        } else {
            printf(" %s : %zu\n", it->first.c_str(), it->second.current);
        }
    }
}

#define CATCH() catch(std::exception& e){printf("Error %s\n", e.what());}

void refshow(int lvl)
{
    try {
        epics::RefSnapshot snap;
        snap.update();

        showRefs(snap, lvl, false);
    }CATCH()
}

// No locking.  assume only interactive iocsh use
static epics::RefSnapshot savedSnap;

void refsave()
{
    try {
        epics::RefSnapshot snap;
        snap.update();

        savedSnap.swap(snap);
    }CATCH()
}

void refdiff(int lvl)
{
    epics::RefSnapshot snap;
    snap.update();

    showRefs(snap-savedSnap, lvl, true);
}

static epics::RefMonitor gblmon;

void refmon(double period, int lvl)
{
    if(period==0) {
        gblmon.stop();
    } else if(period>0) {
        gblmon.start(period);
    }
}

} // namespace

namespace epics {namespace pvAccess {

void refTrackRegistrar()
{
    epics::iocshRegister<int, &refshow>("refshow", "detail level");
    epics::iocshRegister<&refsave>("refsave");
    epics::iocshRegister<int, &refdiff>("refdiff", "detail level");
    epics::iocshRegister<double, int, &refmon>("refmon", "update period", "detail level");
}

}}

extern "C" {
    using namespace epics::pvAccess;
    epicsExportRegistrar(refTrackRegistrar);
}
