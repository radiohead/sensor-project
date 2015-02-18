all: sensor-mote

CONTIKI=/home/user/contiki-2.7
WITH_UIP6=1
UIP_CONF_IPV6=1

CFLAGS+= -DUIP_CONF_IPV6_RPL
PROJECTDIRS += ../common

include $(CONTIKI)/Makefile.include
