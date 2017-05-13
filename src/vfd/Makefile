#
# Mnemonic:	Makefile
# Abstract:	Builds vfd and related components
#			CAUTION: this imports dpdk make things and behaviour is thus somewhat
#					non-standard/unpredictable at times.
# Author:	Alex Zelezniak
# Date:		February 2016
# Mods:		28 Oct 2016 - Add version string based on commit
# -------------------------------------------------------------------------------------

BNXT_SUPPORT = 1

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
APP = vfd

# Build the version string. Would have been better to do in a local script
# but dpdk make process shifts things away to the point that invoking a 
# script here seemed even more dodgy.
#

VFD_VERSION = $(shell junk=$$( (git log -n 1 2>/dev/null || echo Commit non-git-build ) | awk '/^[Cc]ommit / { cid=$$NF; exit(0); } /Date:/ {$$1=""; d=$$0} END { printf( "%s|%s\n", cid, d ) }' );\
	 cid="$${junk%%\|*}";\
	 date="$${junk\#\#*\|}";\
	 tag=$$( git tag -l --contains $$cid 2>/dev/null | head -1 );\
	 mod=$$( git status 2>/dev/null | awk '/modified:/ { m="+++" } END { print m }' );\
	 printf "%s%s%s\n" "$$mod" "$$cid($$tag)" "$$date"; )
	


# all source are stored in SRCS-y	(again, for the dpdk mk file)
#SRCS-y := main.c sriov.c /usr/local/lib/libconfig.a
SRCS-y := main.c sriov.c qos.c vfd_rif.c vfd_dcb.c vfd_i40e.c vfd_ixgbe.c vfd_bnxt.c $(libvfd) $(libjsmn)

CFLAGS += $(WERROR_FLAGS) -I $(PWD)/../lib/ -I $(RTE_SDK)
#-lconfig

# workaround for a gcc bug with noreturn attribute
# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=12603
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_main.o += -Wno-return-type
endif


EXTRA_CFLAGS += -O3 -g -Wfatal-errors #-DVFD_VERSION='"($VFD_VERSION)"'


ifneq ($(BNXT_SUPPORT),)
EXTRA_CFLAGS += -DBNXT_SUPPORT
endif

include $(RTE_SDK)/mk/rte.extapp.mk


clean: 
	rm -rf build

verify:
	echo "$(VFD_VERSION)"
