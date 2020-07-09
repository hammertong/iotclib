VERSION='"0.0.2"'

include options-config.mk
include config-${TARGET}.mk

ifdef DEBUG	
	CFLAGS+= -g3 -O0 -finstrument-functions -rdynamic
	DEBUG=-DDEBUG=1
else
	DEBUG=
endif

ifdef CROSS
	CC=$(CROSS)gcc
endif # CROSS

STRIP=$(CROSS)strip
CFLAGS+= -Wall -I.

SOURCES=$(wildcard *.c)
OBJECTS=$(patsubst %.c, %.o, $(SOURCES))
EXECUTABLE=device

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	@echo "Link executable: $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)
ifndef DEBUG
	@echo "Strip executable: $@"
	@$(STRIP) $@
endif
	@cp -p device client

library: $(OBJECTS)
	@echo "Creating static library"
	@$(AR) rcs libiotc-${TARGET}.a $^
	@$(AR) -M < libiotc-${TARGET}.mri
	
%.o: %.c
	@echo "Make object: $<"
	@$(CC) $(CFLAGS) -c $< -DVERSION=$(VERSION) $(DEBUG) $(EXTRA)

.PHONY: clean

clean:
	@rm -f *.o *.a *.so *.so.* $(EXECUTABLE) client
	@echo "Removing *.o, libraries and $(EXECUTABLE) executables"
