#ifndef _PVACCESSMB_H_
#define _PVACCESSMB_H_

#ifdef epicsExportSharedSymbols
#   define pvAccessMBEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/mb.h>

#ifdef pvAccessMBEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef pvAccessMBEpicsExportSharedSymbols
#endif

MB_DECLARE_EXTERN(channelGet);

#endif
