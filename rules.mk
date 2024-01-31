UC_TOOLS ?= ../uc_tools
GIT_VERSION ?= "unknown"

# This is a mixed C/Rust project.  All rust code is collected in a
# single static lib which is built using cargo.  This rule depends
# only on rs/src/*.rs

# Rustup is no longer managed in this build file.  All build
# dependencies are now managed by synth_tools/flake.nix

# The Rust static libraries are built by cargo.  Build rules are set
# up here to both work for nix build invoking cargo (see flake.nix),
# or rules.mk build invoking cargo directly (e.g. from nix develop
# toolchain).
RS_A_STM ?= rs/target/thumbv6m-none-eabi/release/libsynth_tools_rs.a
RS_A_HOST ?= rs/target/x86_64-unknown-linux-gnu/release/libsynth_tools_rs.a
RS_SRC := \
	rs/src/lib.rs

$(RS_A_HOST): $(RS_SRC)
	@echo $(RS_A_HOST)
	cd rs ; cargo build --release --target x86_64-unknown-linux-gnu
	ls -l $@

$(RS_A_STM): $(RS_SRC)
	@echo $(RS_A_STM)
	cd rs ; cargo build --release --target thumbv6m-none-eabi
	ls -l $@

# Same for Zig
ZIG_LIB=synth_tools_zig
ZIG_A_STM:=FIXME
ZIG_A_HOST:=zig/lib$(ZIG_LIB).a
ZIG_SRC := \
	zig/synth_tools_zig.zig

$(ZIG_A_HOST): $(ZIG_SRC)
	@echo $(ZIG_A_HOST)
	@echo "make: Entering directory '$$(pwd)/zig'"
	cd zig ; zig build-lib $(ZIG_LIB).zig


# Note that bl_midi does not use the A/B partitions with crc, just the
# original x8 slot.

# See synth_tools/builder.sh in /etc/net
# This is moved here as a target simplify updating.
STUDIO_ELF := \
	linux/clock.dynamic.host.elf \
	linux/hub.dynamic.host.elf \
	linux/pd.dynamic.host.elf \
	linux/synth.dynamic.host.elf \
	linux/envy24.dynamic.host.elf \
	linux/jack_netsend.dynamic.host.elf \
	linux/jack_snapshot.dynamic.host.elf \
	linux/jack_midi.dynamic.host.elf \
	linux/jack_control.dynamic.host.elf \
	linux/a2jmidid.dynamic.host.elf \

STM_ELF := \
	stm32f103/bl_midi_bp.core.f103.elf \
	stm32f103/bl_midi_bp_mod.core.f103.elf \
	stm32f103/synth.x8.f103.elf \
	stm32f103/synth.x8.f103.bin \
	stm32f103/pixi.x8.f103.elf \
	stm32f103/pixi.x8.f103.bin \
	stm32f103/pixi.x8.f103.dasm \

STM_ELF_DIS := \
	stm32f103/bp2.x8.f103.elf \
	stm32f103/console.x8.f103.bin \
	$(UC_TOOLS)/gdb/test_3if.x8ram.f103.bin \

HOST_ELF := \
	linux/test_pdm.dynamic.host.elf \
	linux/test_bl_midi.dynamic.host.elf \
	linux/test_cproc.dynamic.host.elf \
	linux/jack_netsend.dynamic.host.elf \
	linux/jack_info.dynamic.host.elf \
	linux/jack_midi.dynamic.host.elf \
	linux/jack_control.dynamic.host.elf \
	linux/jack_snapshot.dynamic.host.elf \
	linux/synth.dynamic.host.elf \
	linux/hub.dynamic.host.elf \
	linux/akai_fire.dynamic.host.elf \
	linux/clock.dynamic.host.elf \
	linux/pd.dynamic.host.elf \
	linux/envy24.dynamic.host.elf \
	linux/test_sequencer.dynamic.host.elf \
	linux/gen_max11300.dynamic.host.elf \
	linux/tether_bl_midi.dynamic.host.elf \
	linux/synth_tools.dynamic.host.so \
	linux/a2jmidid.dynamic.host.so \

# Subset used by exo studio.
STUDIO_ELF := \
	linux/hub.dynamic.host.elf \
	linux/clock.dynamic.host.elf \
	linux/pd.dynamic.host.elf \
	linux/a2jmidid.dynamic.host.so \

HOST_CRUST_ELF := \
	linux/test_rs.crust.host.elf \


HOST_ELF_DIS := \



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
	$(UC_TOOLS)/tag_u32.host.o \

# For later auto-generated files
GEN_DEPS_COMMON :=
GEN := $(GEN_DEPS_COMMON)



# FIXME: This is slow on NFS.  Maybe cache this into a file.
# Use a script to list the .d files to make this easier to debug.
DEPS := \
	$(shell echo "DEPS start" >&2) \
	$(shell find -name '*.d') \
	$(shell find $(UC_TOOLS) -name '*.d') \
	$(shell echo "DEPS end" >&2)

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
	export CFLAGS="-Ilinux/ -Istm32f103/ -Igeneric/ -I$(UC_TOOLS)/ -I$(UC_TOOLS)/gdb/ -I$(UC_TOOLS)/linux/" ; \
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

A_STM := stm32f103/lib.f103.a $(RS_A_STM)

%.x8.f103.elf: \
	%.f103.o \
	$(A_STM) \
	stm32f103/x8.f103.ld \
	$(UC_TOOLS)/gdb/registers_stm32f103.f103.o \

	@echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export A="$(A_STM)" ; \
	export ARCH=f103 ; \
	export BUILD=stm32f103/build.sh ; \
	export ELF=$@ ; \
	export LD=stm32f103/x8.f103.ld ; \
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

%.f103.dasm: \
	%.f103.elf \

	@echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export ARCH=f103 ; \
	export BUILD=stm32f103/build.sh ; \
	export DASM=$@ ; \
	export ELF=$< ; \
	export TYPE=dasm ; \
	export UC_TOOLS=$(UC_TOOLS)/ ; \
	$$BUILD 2>&1




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
	export BUILD=linux/build.sh ; \
	export C=$< ; \
	export CFLAGS=\ -std=gnu99\ -Igeneric\ -Ilinux/\ -Istm32f103/\ -I/usr/include/lua5.1\ -I$(UC_TOOLS)/\ -I$(UC_TOOLS)/gdb/\ -I$(UC_TOOLS)/linux/\ -I$${ZWIZWA_DEV}/include\ -DVERSION="\"$(GIT_VERSION)\""; \
	export D=$(patsubst %.o,%.d,$@) ; \
	export FIRMWARE=$$(basename $< .c) ; \
	export O=$@ ; \
	export TYPE=o ; \
	export UC_TOOLS=$(UC_TOOLS)/ ; \
	$$BUILD 2>&1

linux/lib.host.a: $(LIB_HOST_A_OBJECTS)
	@echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export A=linux/lib.host.a ; \
	export BUILD=linux/build.sh ; \
	export OBJECTS="$(LIB_HOST_A_OBJECTS)" ; \
	export TYPE=a ; \
	export UC_TOOLS=$(UC_TOOLS)/ ; \
	$$BUILD 2>&1

## FIXME: The .dynamic.host.elf files are still part of /etc/net pkgs
## synth_tools which does not support Rust build yet.  Binaries that
## need the Rust lib use the %.crust.host.elf pattern.

# A_HOST := $(ZIG_A_HOST) $(RS_A_HOST)
# A_HOST := $(RS_A_HOST)

%.dynamic.host.elf: \
	%.host.o \
	linux/lib.host.a \
        $(A_HOST) \

	@echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export A=linux/lib.host.a ; \
	export ARCH=host ; \
	export BUILD=linux/build.sh ; \
	export ELF=$@ ; \
	export LD=linux/dynamic.host.ld ; \
	export MAP=$(patsubst %.elf,%.map,$@) ; \
	export O=$< ; \
	export LDLIBS="$(A_HOST) -Wl,--gc-sections -lpthread -ljack -lasound" ; \
	export TYPE=elf ; \
	export UC_TOOLS=$(UC_TOOLS)/ ; \
	$$BUILD 2>&1

# (synth_tools) tom@tp:/i/exo/synth_tools$ make linux/test_rs.crust.host.elf
%.crust.host.elf: \
	%.host.o \
	linux/lib.host.a \
        $(RS_A_HOST) \

	@echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export A=linux/lib.host.a ; \
	export ARCH=host ; \
	export BUILD=linux/build.sh ; \
	export ELF=$@ ; \
	export LD=linux/dynamic.host.ld ; \
	export MAP=$(patsubst %.elf,%.map,$@) ; \
	export O=$< ; \
	export LDLIBS="$(RS_A_HOST) -Wl,--gc-sections -lpthread -ljack -lasound" ; \
	export TYPE=elf ; \
	export UC_TOOLS=$(UC_TOOLS)/ ; \
	$$BUILD 2>&1

%.dynamic.host.so: \
	%.host.o \
	linux/lib.host.a \

	@echo $@ ; if [ -f env.sh ] ; then . ./env.sh ; fi ; \
	export A=linux/lib.host.a ; \
	export ARCH=host ; \
	export BUILD=linux/build.sh ; \
	export SO=$@ ; \
	export LD=linux/dynamic.host.ld ; \
	export LDLIBS="-lsqlite3" ; \
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
	$(STM_ELF) \
	$(HOST_ELF) \
	$(HOST_CRUST_ELF) \
	stm32f103/x8.f103.ld \
	stm32f103/lib.f103.a \
	linux/lib.host.a \
