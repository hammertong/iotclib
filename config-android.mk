SYSROOT=/usr/local/android-ndk-r10e/platforms/android-9/arch-arm
PREFIX=${SYSROOT}/usr
CC=gcc
CROSS=arm-linux-androideabi-
CFLAGS=-fPIE -fPIC -DDEBUG \
	--sysroot=${SYSROOT} \
	-I${PREFIX}/include \
	-I${PREFIX}/include/glib/glib \
	-I${PREFIX}/include/glib/include \
	-I${PREFIX}/include/glib-2.0 \
	-I${PREFIX}/lib/glib-2.0/include \
	-I${PREFIX}/include/gssdp-1.0 \
	-DIFADDRS_NOT_SUPPORTED=1
MQTT_LIBS=../libs.android/12_mosquitto_1.4.2/lib
MQTT_OBJS=${MQTT_LIBS}/will_mosq.o \
	${MQTT_LIBS}/util_mosq.o \
	${MQTT_LIBS}/tls_mosq.o \
	${MQTT_LIBS}/time_mosq.o \
	${MQTT_LIBS}/thread_mosq.o \
	${MQTT_LIBS}/srv_mosq.o \
	${MQTT_LIBS}/socks_mosq.o \
	${MQTT_LIBS}/send_mosq.o \
	${MQTT_LIBS}/send_client_mosq.o \
	${MQTT_LIBS}/read_handle_shared.o \
	${MQTT_LIBS}/read_handle.o \
	${MQTT_LIBS}/read_handle_client.o \
	${MQTT_LIBS}/net_mosq.o \
	${MQTT_LIBS}/mosquitto.o \
	${MQTT_LIBS}/messages_mosq.o \
	${MQTT_LIBS}/memory_mosq.o \
	${MQTT_LIBS}/logging_mosq.o
LDFLAGS=${MQTT_OBJS} -L${PREFIX}/lib
LIBS=-lz -ldl -lm -lsoup-2.4 -lnice -lgthread-2.0 -lgio-2.0 \
	-lgobject-2.0 -lglib-2.0 -lffi -lgmodule-2.0 -lgio-2.0 -lgssdp-1.0 -lsoup-2.4 \
	-lgio-2.0 -lxml2 -luuid -lglib-2.0 -liconv -lintl -lssl -lcrypto
