d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), \
				kvstore.cc lockserver.cc txnstore.cc versionstore.cc commstore.cc)

LIB-store-backend := $(o)kvstore.o $(o)lockserver.o $(o)txnstore.o $(o)versionstore.o $(o)commstore.o

include $(d)tests/Rules.mk
