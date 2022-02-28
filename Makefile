SRCDIR ?= /opt/fpp/src
include ${SRCDIR}/makefiles/common/setup.mk
include $(SRCDIR)/makefiles/platform/*.mk

all: libfpp-brightness.$(SHLIB_EXT)
debug: all

OBJECTS_fpp_Brightness_so += src/FPPBrightness.o
LIBS_fpp_Brightness_so += -L${SRCDIR} -lfpp -ljsoncpp -lhttpserver
CXXFLAGS_src/FPPBrightness.o += -I${SRCDIR}

%.o: %.cpp Makefile
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

libfpp-brightness.$(SHLIB_EXT): $(OBJECTS_fpp_Brightness_so) ${SRCDIR}/libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_Brightness_so) $(LIBS_fpp_Brightness_so) $(LDFLAGS) -o $@

clean:
	rm -f libfpp-brightness.so $(OBJECTS_fpp_Brightness_so)
