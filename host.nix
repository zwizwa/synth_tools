# Produces:
# - shell for use with cached-nix-shell on a nixos host
# - env for deploying the build tools on a non-nixos host or a sandbox without nix tools
let
  pkgs = import (import ./nix/nixpkgs.nix) { };
  i686 = pkgs.pkgsCross.i686-embedded.buildPackages;

  tools = with pkgs; [
    coreutils bash findutils gnumake
    nasm
    i686.gcc        # i686-elf-gcc
    i686.binutils   # i686-elf-ld
  ];
in {
  # development
  shell = pkgs.mkShell { packages = tools; };

  # deployable: result/bin with all tools
  env = pkgs.buildEnv {
    name = "synth_tools_env";
    paths = tools;
    pathsToLink = [ "/bin" ];
  };
}
