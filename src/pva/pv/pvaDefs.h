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
typedef struct {
    char value[12];
} GUID;

typedef epicsInt32 pvAccessID;

}}

#endif // PVADEFS_H
