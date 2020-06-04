PREFIX=/usr/local/urmetiotc_x86_64
CC=gcc
CROSS=

OPTIONS=
OPTIONS+=-DDEBUG
OPTIONS+=-DSERVER_NAME="\"www.cloud.elkron.com\""
OPTIONS+=-DIOTC_VHOST="\"www.cloud.elkron.com\""
OPTIONS+=-DIOTC_VHOST_PORT=4343
OPTIONS+=-DWEBSERVICE_AUTHFORM="\"httpd_username=dileo&httpd_password=dileo\"" 

# comment for standard behaviour (mqtt assigned by cloud)
#OPTIONS+=-DFORCE_MQTT_BROKER="\"35.195.29.62\""
#OPTIONS+=-DFORCE_MQTT_BROKER="\"35.233.36.87\""
OPTIONS+=-DFORCE_MQTT_BROKER="\"35.205.173.130\""
#OPTIONS+=-DFORCE_MQTT_BROKER="\"35.205.38.87\""

CFLAGS=${OPTIONS} \
	-I${PREFIX}/include \
	-I${PREFIX}/include/glib/glib \
	-I${PREFIX}/include/glib/include \
	-I${PREFIX}/include/glib-2.0 \
	-I${PREFIX}/lib/glib-2.0/include \
	-I${PREFIX}/include/gssdp-1.0
LDFLAGS=-L${PREFIX}/lib
LIBS=-lz -ldl -lm -lsoup-2.4 -lnice -lgthread-2.0 -lgio-2.0 \
	-lgobject-2.0 -lglib-2.0 -lffi -lgmodule-2.0 -lgio-2.0 -lgssdp-1.0 -lsoup-2.4 \
	-lgio-2.0 -lxml2 -luuid -lglib-2.0 -lssl -lcrypto -lpthread -lmosquitto
