#ifndef CLIENTPVT_H
#define CLIENTPVT_H

#include <pva/client.h>

namespace pvac{namespace detail{

void registerRefTrack();
void registerRefTrackGet();
void registerRefTrackMonitor();
void registerRefTrackRPC();

}} // namespace pvac::detail

#endif // CLIENTPVT_H
