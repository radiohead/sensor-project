#include "sensor-mote.h"

static struct uip_udp_conn *udp_server_connection;
static uip_ipaddr_t server_address;

PROCESS(sensor_mote_process, "Sensor mote process");
AUTOSTART_PROCESSES(&sensor_mote_process);

static void send_data(void) {
  uint16_t light_intensity;
  uint16_t temperature;

  light_intensity = light_ziglet_read();
  temperature = (uint16_t) (-39.60 + 0.01 * sht11_temp());

  sensor_packet data = { temperature, light_intensity };
  uip_udp_packet_sendto(udp_server_connection, (void *)&data, sizeof(sensor_packet), &server_address, UIP_HTONS(UDP_SERVER_PORT));
}

static void configure_ipv6_addresses(void) {
  uip_ipaddr_t ipaddr;

  /* Set client address according to client MAC */
  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  /* Set server address according to its MAC */
  uip_ip6addr(&server_address, 0xaaaa, 0, 0, 0, 0xc30c, 0, 0, 1);
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

  light_ziglet_init();
  sht11_init();

  /* TODO: figure out the power */
  cc2420_set_txpower(31);
  //configure_ipv6_addresses();
  //establish_udp_connection();

  etimer_set(&send_timer, SEND_PERIOD);

  while(1) {
    PROCESS_YIELD();

    if (etimer_expired(&send_timer)) {
      /* Enable radio */
      // NETSTACK_MAC.on();

      etimer_reset(&send_timer);
      ctimer_set(&backoff_timer, SEND_PERIOD, send_data, NULL);
    }
    // /* Disable radio */
    // NETSTACK_MAC.off(0);
  }

  PROCESS_END();
}


