CFLAGS += $(HDF5_CFLAGS)
CFLAGS += $(GEOTIFF_CFLAGS)
CFLAGS += $(HDF5_CFLAGS)
# Makefile for pslr
# Module Author: Rudi Gens

include ../../make_support/system_rules

CFLAGS += \
	$(GSL_CFLAGS) \
	$(PROJ_CFLAGS) \
	$(GLIB_CFLAGS)

LIBS  = \
	$(LIBDIR)/asf_fft.a \
	$(LIBDIR)/asf_meta.a \
	$(LIBDIR)/libasf_proj.a \
	$(LIBDIR)/asf.a \
	$(GSL_LIBS) \
	$(PROJ_LIBS) \
	$(GLIB_LIBS) \
	$(FFT_LIBS) \
	$(XML_LIBS) \
	-lm

OBJS  = pta.o \
	fftw.o \
	utilities.o

all: pta clean

pta: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o pta $(OBJS) $(LIBS)
	mv pta$(BIN_POSTFIX) $(BINDIR)

clean:
	rm -f core *.o
