#ifndef __COMMON_H__
  #include "contiki.h"

  #define UDP_CLIENT_PORT 8765
  #define UDP_SERVER_PORT 5678
  
  typedef struct {
    int16_t tempint;
    uint16_t tempfrac;
    char minus;     
  } temp_t;
  
  typedef struct {
    temp_t temperature;
    uint16_t light_intensity;
  } sensor_packet;
  
  /*
  typedef struct {
    uint16_t temperature;
    uint16_t light_intensity;
  } sensor_packet;
  */

  #define DEBUG_ENABLED 1
  #define DEBUG DEBUG_PRINT
  #include "net/uip-debug.h"
#endif
