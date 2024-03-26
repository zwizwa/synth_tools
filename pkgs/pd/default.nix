{ bash, coreutils, which, stdenv, lib, puredata, callPackage }:

stdenv.mkDerivation rec {
  name = "pd";
  version = "current";
  buildInputs = [ bash coreutils which puredata ];
  pd = puredata;
  pdp = callPackage ../pdp {};
  creb = callPackage ../creb {};
  src = ./.;
  builder = ./builder.sh;
}
