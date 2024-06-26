include ../config.mak

ifeq ($(SGX_STEP_HOME),)
$(error Can't detect sgx-step. Have you run `source oe-env.sh` ?)
endif 

LIB_SGXLKL_BUILD ?= $(BUILD_DIR)

SGX_STEP_INCLUDE=$(SGX_STEP_HOME)
SGX_STEP_LIB=$(SGX_STEP_HOME)/libsgxstep

srcdir = ${SGXLKL_ROOT}/src
musl_srcdir = $(SGXLKL_LIBC_SRC_DIR)/src
sgxlkl_scriptsdir = ../scripts

CC = gcc
CROSS_COMPILE =
RANLIB  = $(CROSS_COMPILE)ranlib

# TODO remove -Wno-error flags
CFLAGS_MAIN = -I$(OE_SDK_INCLUDES) -I$(SGX_STEP_INCLUDE) -Wall -Werror -Wno-error=incompatible-pointer-types -Wno-error=pointer-to-int-cast -Wno-error=strict-aliasing
CFLAGS_MAIN += -DOE_WITH_EXPERIMENTAL_EEID -I$(srcdir)/libjson/include -I$(LIB_SGXLKL_BUILD)/config
# TODO add -nostdinc after relayering work complete
#      cannot use -nostdinc currently as stdatomic.h is included which is missing
CFLAGS_ENCLAVE = -ffixed-r14 -I$(OE_SDK_INCLUDES) -I${OE_SDK_INCLUDES}/openenclave/3rdparty -fPIE -DSGXLKL_ENCLAVE
CFLAGS_ENCLAVE += -DOE_WITH_EXPERIMENTAL_EEID -I$(srcdir)/libjson/include -I$(LIB_SGXLKL_BUILD)/config
LINK_MAIN =

SGXLKL_VERSION = "$(shell cat ${SGXLKL_ROOT}/VERSION)"
SGXLKL_GIT_COMMIT = "$(shell git rev-parse --short HEAD || echo unknown)"
LKL_VERSION = "$(shell make --quiet -C ${SGXLKL_ROOT}/lkl kernelversion)"

CFLAGS_MAIN += $(SGXLKL_CFLAGS_EXTRA) -DSGXLKL_VERSION=\"$(SGXLKL_VERSION)\" -DSGXLKL_GIT_COMMIT=\"$(SGXLKL_GIT_COMMIT)\" -DLKL_VERSION=\"$(LKL_VERSION)\"

# Copied from sgx-lkl-musl/config.mak after ./configure
# TODO: Do not hardcode cflags here
CFLAGS_AUTO = -g -pipe -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections \
-Werror=implicit-function-declaration -Werror=implicit-int -Werror=pointer-sign -fPIC
CFLAGS_C99FSE = -std=c99 -ffreestanding -fexcess-precision=standard -frounding-math -Wa,--noexecstack

LIBVICSETUP_DIR = $(srcdir)/vicsetup/libvicsetup
ARGON2_DIR = $(SGXLKL_ROOT)/third_party/argon2
ARGON2_SRC += $(ARGON2_DIR)/src/argon2.c
ARGON2_SRC += $(ARGON2_DIR)/src/blake2/blake2b.c
ARGON2_SRC += $(ARGON2_DIR)/src/core.c
ARGON2_SRC += $(ARGON2_DIR)/src/encoding.c
ARGON2_SRC += $(ARGON2_DIR)/src/opt.c
ARGON2_SRC += $(ARGON2_DIR)/src/thread.c

CFLAGS_SGXLKL_ALL = $(CFLAGS_C99FSE) $(CFLAGS_AUTO) $(CFLAGS_MAIN) $(MUSL_CFLAGS)
CFLAGS_SGXLKL_ALL += -D_XOPEN_SOURCE=700
CFLAGS_SGXLKL_ALL += $(SGXLKL_INCLUDES) -isystem $(E2FSPROGS_BUILD)/include -isystem $(DEVICEMAPPER_BUILD)/include -I$(srcdir)/libjson/include -I$(LIBVICSETUP_DIR)/include -isystem $(ARGON2_DIR)/include

# -----------------------------------------------------------------------------------------

$(SGX_STEP_LIB)/libsgx-step.a:  
	$(MAKE) -C $(SGX_STEP_HOME)/libsgxstep all 

all: builddirs $(SGXLKL_RUN_TARGET) $(SGXLKL_STATIC_LIB)

$(SGXLKL_RUN_TARGET): builddirs $(LIB_SGXLKL_BUILD)/$(SGXLKL_RUN_TARGET)

$(SGXLKL_STATIC_LIB): builddirs $(LIB_SGXLKL_BUILD)/sgxlkl/$(SGXLKL_STATIC_LIB)

builddirs:
	@mkdir -p $(LIB_SGXLKL_BUILD)/enclave
	@mkdir -p $(LIB_SGXLKL_BUILD)/main-oe
	@mkdir -p $(LIB_SGXLKL_BUILD)/sgxlkl
	@mkdir -p $(LIB_SGXLKL_BUILD)/deploy
	@mkdir -p $(LIB_SGXLKL_BUILD)/config

# Auto-generated enclave config
CFGGEN = $(abspath $(TOOLS)/generate_config.py)
CFGSCHEMA = $(abspath $(TOOLS)/schemas/enclave-config.schema.json)
CFGH = $(LIB_SGXLKL_BUILD)/config/sgxlkl_enclave_config_gen.h
CFGC = $(LIB_SGXLKL_BUILD)/config/sgxlkl_enclave_config_gen.c
$(CFGH): $(CFGGEN) $(CFGSCHEMA)
	cd $(LIB_SGXLKL_BUILD)/config; python3 $(CFGGEN) --header sgxlkl_enclave_config_gen.h --source sgxlkl_enclave_config_gen.c $(CFGSCHEMA)
$(CFGC): $(CFGH)
shared/sgxlkl_enclave_config.c main-oe/serialize_enclave_config.c: $(CFGH)

# Auto-generated host config
HCFGSCHEMA = $(abspath $(TOOLS)/schemas/host-config.schema.json)
HCFGH = $(LIB_SGXLKL_BUILD)/config/sgxlkl_host_config_gen.h
HCFGC = $(LIB_SGXLKL_BUILD)/config/sgxlkl_host_config_gen.c
$(HCFGH): $(CFGGEN) $(HCFGSCHEMA)
	cd $(LIB_SGXLKL_BUILD)/config; python3 $(CFGGEN) --header sgxlkl_host_config_gen.h --source sgxlkl_host_config_gen.c $(HCFGSCHEMA)
$(HCFGC): $(HCFGH)
main-oe/sgxlkl_host_config.c main-oe/sgxlkl_run_oe.c main-oe/sgxlkl_util.c: $(HCFGH)

# Build enclave static library: libsgxlkl.a

SGXLKL_SRCS = $(wildcard enclave/*.c) $(wildcard shared/*.c) $(wildcard sched/*.c) \
              $(wildcard legacy_host_interface/*.c) $(wildcard lkl/*.c) $(wildcard wireguard/*.c) $(wildcard attestation/*.c) $(wildcard libjson/*.c) \
	      $(CFGC) \
              $(wildcard $(LIBVICSETUP_DIR)/*.c) \
			  ${ARGON2_SRC}

OE_ENCLAVE_CALLS = enclave/sgxlkl_t.c
ifeq (,$(findstring ${OE_ENCLAVE_CALLS},$(SGXLKL_SRCS)))
  SGXLKL_SRCS += ${OE_ENCLAVE_CALLS}
endif

SGXLKL_OBJS=$(addprefix $(LIB_SGXLKL_BUILD)/sgxlkl/,$(SGXLKL_SRCS:.c=.o))

${OE_ENCLAVE_CALLS}: sgxlkl.edl
	${OE_OESIGN_TOOL_PATH}/oeedger8r --search-path $(OE_SDK_INCLUDES) \
		--trusted $< --trusted-dir enclave/
	mv enclave/sgxlkl_t.h include/enclave/; mv enclave/sgxlkl_args.h include/shared/
	sed -i '/#include "sgxlkl_t.h"/c\#include "enclave/sgxlkl_t.h"' enclave/sgxlkl_t.c
	sed -i '/#include "sgxlkl_args.h"/c\#include "shared/sgxlkl_args.h"' enclave/sgxlkl_t.c
	sed -i '/#include "sgxlkl_args.h"/c\#include "shared/sgxlkl_args.h"' include/enclave/sgxlkl_t.h

$(SGXLKL_OBJS): $(LIB_SGXLKL_BUILD)/sgxlkl/%.o: %.c | $(SGXLKL_LIBC_BLD_DIR)/include ${OE_ENCLAVE_CALLS}
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) -I$(srcdir)/include -I$(OE_HOME)/include $(CFLAGS_SGXLKL_ALL) $(CFLAGS_ENCLAVE) -c -o $@ $<

$(LIB_SGXLKL_BUILD)/sgxlkl/$(SGXLKL_STATIC_LIB): $(SGXLKL_OBJS) builddirs
	@echo "AR rc $@"
	@$(AR) rc $@ $(SGXLKL_OBJS)
	@echo "RANLIB $@"
	@$(RANLIB) $@

# Build run tool: sgx-lkl-run-oe

MAIN_OE_SRCS = $(wildcard main-oe/*.c) $(wildcard shared/*.c) $(wildcard host_interface/*.c) $(wildcard libjson/*.c) $(CFGC) $(HCFGC)

OE_HOST_CALLS = main-oe/sgxlkl_u.c
ifeq (,$(findstring ${OE_HOST_CALLS},$(MAIN_OE_SRCS)))
  MAIN_OE_SRCS += ${OE_HOST_CALLS}
endif

MAIN_OE_OBJS =$(addprefix $(LIB_SGXLKL_BUILD)/main-oe/,$(MAIN_OE_SRCS:.c=.o))

OE_PKGCONFIG_OUTPUT := $(shell PKG_CONFIG_PATH=$(OE_SDK_ROOT)/share/pkgconfig:${PKG_CONFIG_PATH} pkg-config oehost-gcc --libs)
ifeq ($(findstring sgx_dcap_ql,$(OE_PKGCONFIG_OUTPUT)),sgx_dcap_ql)
	DCAP_LIB=-lsgx_dcap_ql
endif

${OE_HOST_CALLS}: sgxlkl.edl
	${OE_OESIGN_TOOL_PATH}/oeedger8r --search-path $(OE_SDK_INCLUDES) \
		--untrusted $< --untrusted-dir main-oe/
	mv main-oe/sgxlkl_u.h include/host/; mv main-oe/sgxlkl_args.h include/shared/
	sed -i '/#include "sgxlkl_u.h"/c\#include "host/sgxlkl_u.h"' main-oe/sgxlkl_u.c
	sed -i '/#include "sgxlkl_args.h"/c\#include "shared/sgxlkl_args.h"' main-oe/sgxlkl_u.c
	sed -i '/#include "sgxlkl_args.h"/c\#include "shared/sgxlkl_args.h"' include/host/sgxlkl_u.h

$(MAIN_OE_OBJS): $(LIB_SGXLKL_BUILD)/main-oe/%.o: %.c | ${OE_HOST_CALLS}
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) -I${LKL_BUILD}/include/ -I$(srcdir)/include -I$(srcdir)/main-oe/ $(CFLAGS_MAIN) -c -o $@ $<

$(LIB_SGXLKL_BUILD)/$(SGXLKL_RUN_TARGET): $(SGX_STEP_LIB)/libsgx-step.a $(LIB_SGXLKL_BUILD)/main-oe/main-oe.a $(OE_SDK_LIBS)/openenclave/host/liboehost.a builddirs
	@echo "CC $<"
	@$(CC) $(CFLAGS_MAIN) -o $@ $(LIB_SGXLKL_BUILD)/main-oe/main-oe.a -lcrypto $(LINK_MAIN) \
	-pthread -ldl -lrt -lz -lm -Wl,-T $(sgxlkl_scriptsdir)/sgx-lkl.ld -L$(SGX_STEP_LIB) -L$(OE_SDK_LIBS)/openenclave/host -rdynamic \
	-Wl,-z,noexecstack -lsgx-step -loehost -ldl -lsgx_enclave_common -lsgx_urts -lssl -lcrypto $(DCAP_LIB)

$(LIB_SGXLKL_BUILD)/main-oe/main-oe.a: $(MAIN_OE_OBJS) builddirs
	@echo "AR rc $@"
	@$(AR) rc $@ $(MAIN_OE_OBJS)
	@echo "RANLIB $@"
	@$(RANLIB) $@

$(SGXLKL_LIBC_BLD_DIR)/include:
	[ "$(GLIBC_BUILD)" = "true" ] || $(MAKE) -C $(SGXLKL_LIBC_SRC_DIR) install-headers >/dev/null

clean:
	rm -rf $(LIB_SGXLKL_BUILD)/enclave
	rm -rf $(LIB_SGXLKL_BUILD)/main-oe
	rm -rf $(LIB_SGXLKL_BUILD)/sgxlkl
	rm -rf $(LIB_SGXLKL_BUILD)/config
	rm -rf $(LIB_SGXLKL_BUILD)/$(SGXLKL_RUN_TARGET)
	rm -rf $(srcdir)/main-oe/sgxlkl_u.c $(srcdir)/include/host/sgxlkl_u.h $(srcdir)/enclave/sgxlkl_t.c $(srcdir)/include/enclave/sgxlkl_t.h

.PHONY: clean builddirs
