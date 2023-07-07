# This binds standar nixpkgs package descriptions in pkgs/* with the
# toplevel nixpkgs repo (zwizwa mod against old unstable), and some
# overlays recorded in this repository.

{}:
let
pkgs =
	import (builtins.fetchTarball
    https://github.com/zwizwa/nixpkgs/archive/zwizwa.tar.gz
  )
    {
      overlays = [
        # (self: super: {lua           = pkgs.callPackage ./pkgs/lua {};})
        # (self: super: {libopencm3    = pkgs.callPackage ./pkgs/libopencm3 {};})
        # (self: super: {openocd       = pkgs.callPackage ./pkgs/openocd {};})
        # (self: super: {arm-eabi-gdb  = pkgs.callPackage ./pkgs/arm-eabi-gdb {};})
        # (self: super: {saleae        = pkgs.callPackage ./pkgs/saleae {};})
      ];
    };
in pkgs.callPackage ./pkgs/dev {}
