{}:
let
pkgs =
	import (builtins.fetchTarball
    # https://github.com/zwizwa/nixpkgs/archive/185f435e47804014e408936d386bc6baf5358083.tar.gz
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
