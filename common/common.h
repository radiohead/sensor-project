#ifndef __COMMON_H__
  #include "contiki.h"

  #define UDP_CLIENT_PORT 8765
  #define UDP_SERVER_PORT 5678

  typedef struct {
    uint16_t temperature;
    uint16_t light_intensity;
  } sensor_packet;
#endif
