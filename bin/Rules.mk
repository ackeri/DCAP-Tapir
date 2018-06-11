d := $(dir $(lastword $(MAKEFILE_LIST)))

#client lib
$(d)libtapir.so: $(patsubst %.o,%-pic.o, $(OBJS-tapir-client))
LDFLAGS-$(d)libtapir.so += -shared

BINS += $(d)libtapir.so

#timerserver
$(d)timeserver: $(OBJS-timeserver) $(OBJS-vr-replica) $(LIB-udptransport)

BINS += $(d)timeserver

#server
$(d)server: $(OBJS-tapir-server) $(LIB-udptransport) \
		$(OBJS-ir-replica) $(OBJS-tapir-store)

BINS += $(d)server
