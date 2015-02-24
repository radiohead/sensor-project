#ifndef __UDP_CLIENT_H__
  #define __UDP_CLIENT_H__

  #include "dev/temperature-sensor.h"
  #include "dev/light-sensor.h"

  /* Include these header file for light sensor driver */
  #include "dev/i2cmaster.h"
  #include "dev/light-ziglet.h"
  #include "dev/sht11.h"

  #include "dev/cc2420.h"
  #include "lib/random.h"
  #include "net/uip.h"
  #include "net/uip-ds6.h"
  #include "net/uip-udp-packet.h"

  #include <stdio.h>
  #include <string.h>

  #include "common.h"

  #define PERIOD          10
  #define SEND_PERIOD     (PERIOD * CLOCK_SECOND)
  #define MAX_PAYLOAD_LEN 30
#endif
