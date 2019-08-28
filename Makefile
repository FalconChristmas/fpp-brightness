include /opt/fpp/src/makefiles/common/setup.mk

all: libfpp-brightness.so
debug: all

OBJECTS_fpp_Brightness_so += src/FPPBrightness.o
LIBS_fpp_Brightness_so += -L/opt/fpp/src -lfpp
CXXFLAGS_src/FPPBrightness.o += -I/opt/fpp/src

%.o: %.cpp Makefile
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

libfpp-brightness.so: $(OBJECTS_fpp_Brightness_so) /opt/fpp/src/libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_Brightness_so) $(LIBS_fpp_Brightness_so) $(LDFLAGS) -o $@

clean:
	rm -f libfpp-brightness.so $(OBJECTS_fpp_Brightness_so)
