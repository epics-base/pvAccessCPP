#include <stdio.h>
#include <stdlib.h>
#include <rtems/rtems_bsdnet.h>
#include <rtems/error.h>

#include "rtemsNetworking.h"

#include <epicsExit.h>
#include <osdTime.h>

rtems_task
Init (rtems_task_argument ignored)
{
  rtems_bsdnet_initialize_network ();
  //rtems_bsdnet_show_if_stats ();

  rtems_time_of_day timeOfDay;
  if (rtems_clock_get(RTEMS_CLOCK_GET_TOD,&timeOfDay) != RTEMS_SUCCESSFUL) {
    timeOfDay.year = 2014;
    timeOfDay.month = 1;
    timeOfDay.day = 1;
    timeOfDay.hour = 0;
    timeOfDay.minute = 0;
    timeOfDay.second = 0;
    timeOfDay.ticks = 0;

    rtems_status_code ret = rtems_clock_set(&timeOfDay);
    if (ret != RTEMS_SUCCESSFUL) {
      printf("**** Can't set time %s\n", rtems_status_text(ret));
    }
  }
  osdTimeRegister();
  
  extern void pvAccessAllTests(void);
  pvAccessAllTests();
  epicsExit(0);
}
