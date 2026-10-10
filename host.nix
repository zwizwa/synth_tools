# Produces:
# - shell for use with cached-nix-shell on a nixos host
# - env for deploying the build tools on a non-nixos host or a sandbox without nix tools
let

  # The heavily modded exo-nix package set is shared across several
  # projects.  It's too hard to keep tweaking different sets for
  # different projects so use "my machine".
  pkgs = (import ../exo-nix/pkgs.nix);
  i686 = pkgs.pkgsCross.i686-embedded.buildPackages;
  tools = with pkgs; [
    coreutils bash findutils gnumake

    pkg-config
    gcc
    gcc-arm-embedded
    jack2 jack2.dev
    alsa-lib alsa-lib.dev
    fftwFloat fftwFloat.dev
    puredata
    lace
    sqlite.out sqlite.dev sqlite.bin
    rustToolchain
    # a2jmidid
    # erlang
    # pulseaudio
  ];
in {
  # development
  shell = pkgs.mkShell { packages = tools; };

  env = pkgs.buildEnv {
    name = "synth_tools_env";
    paths = tools;
    pathsToLink = [ "/bin" "/include" "/lib" ];
  };
}
