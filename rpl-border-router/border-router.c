/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */
/**
 * \file
 *         border-router
 * \author
 *         Niclas Finne <nfi@sics.se>
 *         Joakim Eriksson <joakime@sics.se>
 *         Nicolas Tsiftes <nvt@sics.se>
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include "net/uip.h"
#include "net/uip-ds6.h"
#include "net/rpl/rpl.h"
#include "net/netstack.h"

#include "dev/button-sensor.h"
#include "dev/slip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define UIP_IP_BUF ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

#include "httpd-simple.h"
#include "common.h"

uint16_t dag_id[] = {0x1111, 0x1100, 0, 0, 0, 0, 0, 0x0011};
static struct uip_udp_conn *udp_connection;
static uip_ipaddr_t prefix;
static uint8_t prefix_set;

PROCESS(border_router_process, "Border router process");
PROCESS(webserver_nogui_process, "Web server");
AUTOSTART_PROCESSES(&border_router_process, &webserver_nogui_process);
PROCESS_THREAD(webserver_nogui_process, ev, data) {
  PROCESS_BEGIN();

  httpd_init();

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event);
    httpd_appcall(data);
  }

  PROCESS_END();
}

typedef struct {
  uint8_t node_id;
  uint16_t temperature;
  uint16_t light_intensity;
} sensor_measurement;

static const char *TOP = "<html><head><title>ContikiRPL</title></head><body>\n";
static const char *BOTTOM = "</body></html>\n";
static sensor_measurement sensor_measurements[] = {
  { 0, 0, 0},
  { 0, 0, 0},
  { 0, 0, 0}
};


static PT_THREAD(generate_sensor_html(struct httpd_state *s)) {
  static int i;
  char str_buf[16];

  PSOCK_BEGIN(&s->sout);

  SEND_STRING(&s->sout, TOP);
  SEND_STRING(&s->sout, "<ul>");
  SEND_STRING(&s->sout, "<li>");
  SEND_STRING(&s->sout, "Node ID");
  SEND_STRING(&s->sout, " - ");
  SEND_STRING(&s->sout, "Temperature");
  SEND_STRING(&s->sout, " - ");
  SEND_STRING(&s->sout, "Light intensity");
  SEND_STRING(&s->sout, "</li>");

  for (i = 0; i < 3; ++i) {
    if (sensor_measurements[i].node_id != 0) {
      SEND_STRING(&s->sout, "<li>");

      sprintf(str_buf, "%u", sensor_measurements[i].node_id);
      SEND_STRING(&s->sout, str_buf);
      SEND_STRING(&s->sout, " - ");

      sprintf(str_buf, "%u", sensor_measurements[i].temperature);
      SEND_STRING(&s->sout, str_buf);
      SEND_STRING(&s->sout, " - ");

      sprintf(str_buf, "%u", sensor_measurements[i].light_intensity);
      SEND_STRING(&s->sout, str_buf);

      SEND_STRING(&s->sout, "</li>");
    }
  }

  SEND_STRING(&s->sout, BOTTOM);

  PSOCK_END(&s->sout);
}

httpd_simple_script_t httpd_simple_get_script(const char *name) {
  return generate_sensor_html;
}

static void print_local_addresses(void) {
  int i;
  uint8_t state;

  PRINTA("Server IPv6 addresses:\n");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      PRINTA(" ");
      uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTA("\n");
    }
  }
}

void request_prefix(void) {
  /* mess up uip_buf with a dirty request... */
  uip_buf[0] = '?';
  uip_buf[1] = 'P';
  uip_len = 2;
  slip_send();
  uip_len = 0;
}

void set_prefix_64(uip_ipaddr_t *prefix_64) {
  uip_ipaddr_t ipaddr;
  memcpy(&prefix, prefix_64, 16);
  memcpy(&ipaddr, prefix_64, 16);
  prefix_set = 1;
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
}

static void handle_sensor_packet(void) {
  uint8_t node_id;
  sensor_packet *data;

  if (uip_newdata()) {
    data = (sensor_packet *)uip_appdata;
    node_id = UIP_IP_BUF->srcipaddr.u8[sizeof(UIP_IP_BUF->srcipaddr.u8) - 1];

    PRINTF("Data recv; temp: %u; light: %u\n", data->temperature, data->light_intensity);
    PRINTF("From: %d\n", node_id);

    int i;
    int node_found = 0;
    for (i = 0; i < 3; ++i) {
      if (sensor_measurements[i].node_id == node_id) {
        sensor_measurements[i].temperature = data->temperature;
        sensor_measurements[i].light_intensity = data->light_intensity;

        node_found = 1;
      }
    }

    if (node_found == 0) {
      for (i = 0; i < 3; ++i) {
        if (sensor_measurements[i].node_id == 0) {
          sensor_measurements[i].node_id = node_id;
          sensor_measurements[i].temperature = data->temperature;
          sensor_measurements[i].light_intensity = data->light_intensity;

          break;
        }
      }
    }
  }
}

PROCESS_THREAD(border_router_process, ev, data) {
  static struct etimer et;
  rpl_dag_t *dag;
  #if DEBUG_ENABLED
    static struct etimer energy_timer;
    static double energy_consumed;
  #endif

  PROCESS_BEGIN();

/* While waiting for the prefix to be sent through the SLIP connection, the future
 * border router can join an existing DAG as a parent or child, or acquire a default
 * router that will later take precedence over the SLIP fallback interface.
 * Prevent that by turning the radio off until we are initialized as a DAG root.
 */
  prefix_set = 0;
  NETSTACK_MAC.off(0);

  PROCESS_PAUSE();
  SENSORS_ACTIVATE(button_sensor);
  PRINTF("RPL-Border router started\n");

  while(!prefix_set) {
    etimer_set(&et, CLOCK_SECOND);
    request_prefix();
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  }

  dag = rpl_set_root(RPL_DEFAULT_INSTANCE,(uip_ip6addr_t *)dag_id);
  if(dag != NULL) {
    rpl_set_prefix(dag, &prefix, 64);
    PRINTF("created a new RPL dag\n");
  }

  NETSTACK_MAC.off(1);

  // DEBUG
  print_local_addresses();

  udp_connection = udp_new(NULL, UIP_HTONS(UDP_CLIENT_PORT), NULL);
  if (udp_connection == NULL) {
    PRINTF("UDP host failed to establish itself, exiting.\n");
    PROCESS_EXIT();
  }
  udp_bind(udp_connection, UIP_HTONS(UDP_SERVER_PORT));
  PRINTF("UDP host established.\n");

  #if DEBUG_ENABLED
    etimer_set(&energy_timer, CLOCK_SECOND * 10);
  #endif

  while(1) {
    PROCESS_YIELD();

    #if DEBUG_ENABLED
      if (etimer_expired(&energy_timer)) {
        energy_consumed = (energest_type_time(ENERGEST_TYPE_CPU)) * (500 * 3) +
                  (energest_type_time(ENERGEST_TYPE_LPM)) * (0.005 * 3) +
                  (energest_type_time(ENERGEST_TYPE_TRANSMIT)) * (174 * 3) +
                  (energest_type_time(ENERGEST_TYPE_LISTEN)) * (188 * 3);

        printf("Energy consumed per 10 seconds: %u uJ\n", ((energy_consumed / 10) / RTIMER_SECOND));
        etimer_set(&energy_timer, CLOCK_SECOND * 10);
      }
    #endif

    if (ev == tcpip_event) {
      handle_sensor_packet();
    }

    if (ev == sensors_event && data == &button_sensor) {
      PRINTF("Initiating global repair\n");
      rpl_repair_root(RPL_DEFAULT_INSTANCE);
    }
  }

  PROCESS_END();
}
