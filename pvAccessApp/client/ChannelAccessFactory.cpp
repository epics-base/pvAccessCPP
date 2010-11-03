/*ChannelAccessFactory.cpp*/

#include <lock.h>
#include "pvAccess.h"
#include "pvData.h"
#include "factory.h"

namespace epics { namespace pvAccess {

static ChannelAccess* channelAccess = 0;


 ChannelAccess * getChannelAccess() {
     static Mutex mutex = Mutex();
     Lock guard(&mutex);

     if(channelAccess==0){
          //channelAccess = new ChannelAccessImpl();
     }
     return channelAccess;
 }

}}

