UC_TOOLS ?= ../uc_tools
GIT_VERSION ?= "unknown"

BIN2FW := $(UC_TOOLS)/linux/bin2fw.dynamic.host.elf

STM_ELF := \
	stm32f103/synth.x8ab.f103.fw.elf \
	stm32f103/synth.x8ab.f103.fw.bin \
	stm32f103/bp2.x8ab.f103.fw.elf \
	stm32f103/console.x8ab.f103.fw.bin \
	stm32f103/bl_open.core.f103.elf \
	stm32f103/bl_midi.core.f103.elf \
	$(UC_TOOLS)/gdb/test_3if.x8ram.f103.bin \

STM_ELF_DIS := \

HOST_ELF := \
        $(BIN2FW) \
	tools/test_pdm.dynamic.host.elf \
	tools/tether_bl.dynamic.host.elf \

ALL_ELF := $(STM_ELF) $(HOST_ELF)

LIB_F103_A_OBJECTS := \
	$(UC_TOOLS)/gdb/bootloader.f103.o \
	$(UC_TOOLS)/gdb/cdcacm_desc.f103.o \
	$(UC_TOOLS)/gdb/gdbstub.f103.o \
	$(UC_TOOLS)/gdb/hw_bootloader.f103.o \
	$(UC_TOOLS)/gdb/memory.f103.o \
	$(UC_TOOLS)/gdb/pluginlib.f103.o \
	$(UC_TOOLS)/gdb/rsp_packet.f103.o \
	$(UC_TOOLS)/gdb/sm_etf.f103.o \
	$(UC_TOOLS)/gdb/vector.f103.o \
	$(UC_TOOLS)/gdb/instance.f103.o \
	$(UC_TOOLS)/gdb/stack.f103.o \
	$(UC_TOOLS)/gdb/semihosting.f103.o \
	$(UC_TOOLS)/memoize.f103.o \
	\
	$(UC_TOOLS)/csp.f103.o \
	$(UC_TOOLS)/cbuf.f103.o \
	$(UC_TOOLS)/info_null.f103.o \
	$(UC_TOOLS)/infof.f103.o \
	$(UC_TOOLS)/mdio.f103.o \
	$(UC_TOOLS)/pbuf.f103.o \
	$(UC_TOOLS)/sliplib.f103.o \
	$(UC_TOOLS)/slipstub.f103.o \
	$(UC_TOOLS)/tag_u32.f103.o \
	$(UC_TOOLS)/tools.f103.o \
	$(UC_TOOLS)/cycle_counter.f103.o \
	\


LIB_HOST_A_OBJECTS := \
	$(UC_TOOLS)/cbuf.host.o \
	$(UC_TOOLS)/csp.host.o \
	$(UC_TOOLS)/info_null.host.o \
	$(UC_TOOLS)/infof.host.o \
	$(UC_TOOLS)/mdio.host.o \
	$(UC_TOOLS)/pbuf.host.o \
	$(UC_TOOLS)/sliplib.host.o \
	$(UC_TOOLS)/tools.host.o \
	$(UC_TOOLS)/cycle_counter.host.o \
	$(UC_TOOLS)/linux/packet_bridge.host.o \

# For later auto-generated files
GEN_DEPS_COMMON :=
GEN := $(GEN_DEPS_COMMON)




# Use a script to list the .d files to make this easier to debug.
DEPS := $(shell find -name '*.d') $(shell find $(UC_TOOLS) -name '*.d')
-include $(DEPS)


# STM32F103 (Blue Pill) firmware

# Linker file
stm32f103/%.ld: stm32f103/%.ld.sh
	@echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export BUILD=stm32f103/build.sh ; \
	export LD=$@ ; \
	export LD_GEN=$< ; \
	export TYPE=ld ; \
	export UC_TOOLS=$(UC_TOOLS)/ ; \
	$$BUILD 2>&1


%.f103.o: %.c $(GEN)
	@echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export ARCH=f103 ; \
	export BUILD=stm32f103/build.sh ; \
	export C=$< ; \
	export CFLAGS="-Itools/ -Istm32f103/ -I./ -I$(UC_TOOLS)/ -I$(UC_TOOLS)/gdb/ -I$(UC_TOOLS)/linux/" ; \
	export D=$(patsubst %.o,%.d,$@) ; \
	export FIRMWARE=memory ; \
	export O=$@ ; \
	export TYPE=o ; \
	export UC_TOOLS=$(UC_TOOLS)/ ; \
	$$BUILD 2>&1

# Main object library.  All elf files link against this.
stm32f103/lib.f103.a: $(LIB_F103_A_OBJECTS) rules.mk
	@echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export A=$@ ; \
	export BUILD=stm32f103/build.sh ; \
	export OBJECTS="$(LIB_F103_A_OBJECTS)" ; \
	export TYPE=a ; \
	export UC_TOOLS=$(UC_TOOLS)/ ; \
	$$BUILD 2>&1

# Core binaries.  This is e.g. for bootloader.
%.core.f103.elf: \
	%.f103.o \
	stm32f103/lib.f103.a \
	stm32f103/core.f103.ld \
	$(UC_TOOLS)/gdb/registers_stm32f103.f103.o \

	@echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export A=stm32f103/lib.f103.a ; \
	export ARCH=f103 ; \
	export BUILD=stm32f103/build.sh ; \
	export ELF=$@ ; \
	export LD=stm32f103/core.f103.ld ; \
	export MAP=$(patsubst %.elf,%.map,$@) ; \
	export O=$< ; \
	export TYPE=elf ; \
	export UC_TOOLS=$(UC_TOOLS)/ ; \
	$$BUILD 2>&1

# Application, linked to addresses for partition A and spillover into B.  Only for debugging.
%.x8ab.f103.elf: \
	%.f103.o \
	stm32f103/lib.f103.a \
	stm32f103/x8ab.f103.ld \
	$(UC_TOOLS)/gdb/registers_stm32f103.f103.o \

	@echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export A=stm32f103/lib.f103.a ; \
	export ARCH=f103 ; \
	export BUILD=stm32f103/build.sh ; \
	export ELF=$@ ; \
	export LD=stm32f103/x8ab.f103.ld ; \
	export MAP=$(patsubst %.elf,%.map,$@) ; \
	export O=$< ; \
	export TYPE=elf ; \
	export UC_TOOLS=$(UC_TOOLS)/ ; \
	export VERSION_LINK_GEN=./version.sh ; \
	$$BUILD 2>&1

# RAM image, e.g. loaded using bl_tether.c
%.x8ram.f103.elf: \
	%.f103.o \
	stm32f103/lib.f103.a \
	stm32f103/x8ram.f103.ld \
	$(UC_TOOLS)/gdb/registers_stm32f103.f103.o \

	@echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export A=stm32f103/lib.f103.a ; \
	export ARCH=f103 ; \
	export BUILD=stm32f103/build.sh ; \
	export ELF=$@ ; \
	export LD=stm32f103/x8ram.f103.ld ; \
	export MAP=$(patsubst %.elf,%.map,$@) ; \
	export O=$< ; \
	export TYPE=elf ; \
	export UC_TOOLS=$(UC_TOOLS)/ ; \
	export VERSION_LINK_GEN=./version.sh ; \
	$$BUILD 2>&1


%.f103.fw.elf: \
	%.f103.elf \
	$(BIN2FW) \

	echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export ARCH=f103 ; \
	export BUILD=stm32f103/build.sh ; \
	export ELF=$< ; \
	export FW=$@ ; \
	export TYPE=fw ; \
	export BIN2FW=$(BIN2FW) ; \
	export UC_TOOLS=$(UC_TOOLS)/ ; \
	export ELF_CAS=cas ; \
	$$BUILD 2>/dev/null


# Raw binary from elf
%.bin: \
	%.elf \

	echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export ARCH=f103 ; \
	export BUILD=stm32f103/build.sh ; \
	export ELF=$< ; \
	export BIN=$@ ; \
	export TYPE=bin ; \
	export UC_TOOLS=$(UC_TOOLS)/ ; \
	$$BUILD 2>&1

# HOST

%.host.o: %.c $(GEN)
	@echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export ARCH=host ; \
	export BUILD=tools/build.sh ; \
	export C=$< ; \
	export CFLAGS=\ -std=gnu99\ -Itools/\ -Inano1/\ -Istm32f103/\ -I/usr/include/lua5.1\ -I$(UC_TOOLS)/\ -I$(UC_TOOLS)/gdb/\ -I$(UC_TOOLS)/linux/\ -DVERSION="\"$(GIT_VERSION)\""; \
	export D=$(patsubst %.o,%.d,$@) ; \
	export FIRMWARE=$$(basename $< .c) ; \
	export O=$@ ; \
	export TYPE=o ; \
	export UC_TOOLS=$(UC_TOOLS)/ ; \
	$$BUILD 2>&1


tools/lib.host.a: $(LIB_HOST_A_OBJECTS)
	@echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export A=tools/lib.host.a ; \
	export BUILD=tools/build.sh ; \
	export OBJECTS="$(LIB_HOST_A_OBJECTS)" ; \
	export TYPE=a ; \
	export UC_TOOLS=$(UC_TOOLS)/ ; \
	$$BUILD 2>&1

%.dynamic.host.elf: \
	%.host.o \
	tools/lib.host.a \

	@echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export A=tools/lib.host.a ; \
	export ARCH=host ; \
	export BUILD=tools/build.sh ; \
	export ELF=$@ ; \
	export LD=tools/dynamic.host.ld ; \
	export MAP=$(patsubst %.elf,%.map,$@) ; \
	export O=$< ; \
	export LDLIBS=\ -lgpiod\ -lpthread\ -lluajit-5.1 ; \
	export TYPE=elf ; \
	export UC_TOOLS=$(UC_TOOLS)/ ; \
	$$BUILD 2>&1

%.dynamic.host.so: \
	%.host.o \
	tools/lib.host.a \

	@echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export A=tools/lib.host.a ; \
	export ARCH=host ; \
	export BUILD=tools/build.sh ; \
	export SO=$@ ; \
	export LD=tools/dynamic.host.ld ; \
	export LDLIBS="-lelf -ldw" ; \
	export MAP=$(patsubst %.so,%.map,$@) ; \
	export O=$< ; \
	export TYPE=so ; \
	export UC_TOOLS=$(UC_TOOLS) ; \
	$$BUILD 2>&1

%.dynamic.host.test: \
	%.dynamic.host.elf \

	@echo $@; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	$< > $@




# E.g. for clean target

ALL_PRODUCTS := \
	$(LIB_F103_A_OBJECTS) \
	$(LIB_HOST_A_OBJECTS) \
	$(ALL_ELF) \
	stm32f103/x8.f103.ld \
	stm32f103/lib.f103.a \
	tools/lib.host.a \
