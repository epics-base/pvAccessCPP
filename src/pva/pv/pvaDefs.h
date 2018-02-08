#ifndef PVADEFS_H
#define PVADEFS_H

#include <epicsTypes.h>
#include <epicsMutex.h>
#include <epicsGuard.h>

namespace epics {
namespace pvAccess {

/**
 * Globally unique ID.
 */
struct ServerGUID {
    char value[12];
};

typedef epicsInt32 pvAccessID;

class AtomicBoolean
{
public:
    AtomicBoolean() : val(false) {}

    void set() {
        epicsGuard<epicsMutex> G(mutex);
        val = true;
    }
    void clear() {
        epicsGuard<epicsMutex> G(mutex);
        val = false;
    }

    bool get() const {
        epicsGuard<epicsMutex> G(mutex);
        return val;
    }
private:
    bool val;
    mutable epicsMutex mutex;
};

}}

#endif // PVADEFS_H
