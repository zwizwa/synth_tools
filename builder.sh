#!/bin/sh
source $stdenv/setup

# Needs to build in-place, so copy it from the readonly store.
cp -a $uc_tools uc_tools ; chmod -R a+rwX uc_tools
cp -a $src synth_tools   ; chmod -R a+rwX synth_tools

cd synth_tools
make -j${NIX_BUILD_CORES}

mkdir -p $out/linux
mkdir -p $out/stm32f103

# Copy build products.
cp -a linux/*.elf     $out/linux/
cp -a stm32f103/*.elf $out/stm32f103/

# Rust binaries are built in a separate derivation.  Just link it here.
(cd $out
 ln -s ${RS_LINUX} rs_linux
 ln -s ${PD} pd
)

 
