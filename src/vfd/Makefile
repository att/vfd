
#
# Mnemonic:	Makefile
# Abstract:	Builds vfd and related components
#			CAUTION: this imports dpdk make things and behavour is thus somewhat
#					non-standard/unpredictable at times.
# Author:	Alex Zelezniak
# Date:		February 2016
# -------------------------------------------------------------------------------------

#### change this if needed
#libconf = /usr/lib/x86_64-linux-gnu/libconfig.a
libvfd = $(PWD)/../lib/libvfd.a
libjsmn = $(PWD)/../lib/jsmn/libjsmn.a
headers = $(PWD)/../lib/vfdlib.h

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-vfd-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk


# binary name (all build rules are in rte.extapp.mk in the dpdk clone)
#APP = sriovctl
APP = vfd

# all source are stored in SRCS-y	(again, for the dpdk mk file)
#SRCS-y := main.c sriov.c /usr/local/lib/libconfig.a
SRCS-y := main.c sriov.c qos.c $(libvfd) $(libjsmn)

CFLAGS += $(WERROR_FLAGS) -I $(PWD)/../lib/
#-lconfig

# workaround for a gcc bug with noreturn attribute
# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=12603
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_main.o += -Wno-return-type
endif

EXTRA_CFLAGS += -O3 -g -Wfatal-errors

include $(RTE_SDK)/mk/rte.extapp.mk



	
clean: 
	rm -rf build
