d := $(dir $(lastword $(MAKEFILE_LIST)))

$(d)libtapir.so: $(patsubst %.cc,build/%-pic.o, $(SRCS))
LDFLAGS-$(d)libtapir.so += -shared

BINS += $(d)libtapir.so
