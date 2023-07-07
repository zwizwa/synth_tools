{ pkgs }:
pkgs.stdenv.mkDerivation rec {
  pname = "synth_tools";
  version = "0.0.1";

  src = ./. ;

  buildInputs = [
    pkgs.gcc
    pkgs.jack2
  ];

  configurePhase = ''
  '';

  buildPhase = ''
    make
  '';

  installPhase = ''
    mkdir -p $out/bin
  '';
}
