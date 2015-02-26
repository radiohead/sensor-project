#include "sensor-mote.h"

static struct uip_udp_conn *udp_server_connection;
static uip_ipaddr_t server_address;

PROCESS(sensor_mote_process, "Sensor mote process");
AUTOSTART_PROCESSES(&sensor_mote_process);

static temp_t temperature_read(void) {
  temp_t temp;
  int16_t raw;
  uint16_t absraw;
  int16_t sign;

  sign = 1;
  raw = tmp102_read_temp_raw();
  absraw = raw;

  /* Perform 2C's if sensor returned negative data */
  if (raw < 0) {
    absraw = (raw ^ 0xFFFF) + 1;
    sign = -1;
  }

  temp.tempint = (absraw >> 8) * sign;
  /* Info in 1/10000 of degree */
  temp.tempfrac = ((absraw >> 4) % 16) * 625;
  temp.minus = ((temp.tempint == 0) & (sign == -1)) ? '-' : ' ';

  return temp;
}

static void send_data(void) {
  uint16_t light_intensity;
  temp_t temperature;

  light_intensity = light_ziglet_read();
  temperature = temperature_read();

  PRINTF("Sending data: temperature: %d, light: %d\n", temperature, light_intensity);

  sensor_packet data = { temperature, light_intensity };
  uip_udp_packet_sendto(udp_server_connection, (void *)&data, sizeof(sensor_packet), &server_address, UIP_HTONS(UDP_SERVER_PORT));

  #if DEBUG_ENABLED
    static float energy_consumed;
    energy_consumed = (energest_type_time(ENERGEST_TYPE_CPU)) * (500 * 3) +
                      (energest_type_time(ENERGEST_TYPE_LPM)) * (0.005 * 3) +
                      (energest_type_time(ENERGEST_TYPE_TRANSMIT)) * (174 * 3) +
                      (energest_type_time(ENERGEST_TYPE_LISTEN)) * (188 * 3);

    printf("Energy consumed per cycle: %u mJ\n", ((energy_consumed / 10000) / RTIMER_SECOND));
  #endif
}

static void configure_ipv6_addresses(void) {
  uip_ipaddr_t ipaddr;

  /* Set client address according to client MAC */
  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  /* Set server address according to its MAC */
  uip_ip6addr(&server_address, 0xaaaa, 0, 0, 0, 0xc30c, 0, 0, 0);
}

static void establish_udp_connection(void) {
  do {
    PRINTF("Establishing UDP connection\n");
    udp_server_connection = udp_new(&server_address, UIP_HTONS(UDP_SERVER_PORT), NULL);
  } while (udp_server_connection == NULL);
  udp_bind(udp_server_connection, UIP_HTONS(UDP_CLIENT_PORT));

  PRINTF("UDP connection established with: ");
  PRINT6ADDR(&udp_server_connection->ripaddr);
  PRINTF("on local/remote port %u/%u\n", UIP_HTONS(udp_server_connection->lport), UIP_HTONS(udp_server_connection->rport));
}

PROCESS_THREAD(sensor_mote_process, ev, data) {
  static struct etimer send_timer;
  static struct ctimer backoff_timer;

  PROCESS_BEGIN();
  PROCESS_PAUSE();

  tmp102_init();
  light_ziglet_init();

  configure_ipv6_addresses();
  establish_udp_connection();

  etimer_set(&send_timer, SEND_PERIOD);

  while(1) {
    PROCESS_YIELD();

    if (etimer_expired(&send_timer)) {
      etimer_reset(&send_timer);
      ctimer_set(&backoff_timer, SEND_PERIOD, send_data, NULL);
    }
  }

  PROCESS_END();
}
