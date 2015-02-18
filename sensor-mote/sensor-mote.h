#ifndef __UDP_CLIENT_H__
  #define __UDP_CLIENT_H__

  #include "dev/temperature-sensor.h"
  #include "dev/light-sensor.h"
  #include "dev/cc2420.h"
  #include "lib/random.h"
  #include "net/uip.h"
  #include "net/uip-ds6.h"
  #include "net/uip-udp-packet.h"

  #include <stdio.h>
  #include <string.h>

  #include "common.h"

  #define DEBUG DEBUG_PRINT
  #include "net/uip-debug.h"

  #define PERIOD          30
  #define SEND_PERIOD     (PERIOD * CLOCK_SECOND)
  #define MAX_PAYLOAD_LEN 30
#endif
