PREFIX=/usr/local/urmetiotc_x86_64
CC=gcc
CROSS=
CFLAGS=-DDEBUG -g3 \
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
