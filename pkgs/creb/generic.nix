# This needs some work:
#
# - quicktime?
#
# - v4l error:
# gcc -DPD -Wall -W -Wstrict-prototypes -Wno-unused -Wno-parentheses -Wno-switch -fPIC -O2 -funroll-loops -fomit-frame-pointer -ffast-math -I/nix/store/2vrv6rjfinsx7kdkbd5nzhxrdzwn10vf-v4l-utils-1.22.1-dev/include   -DPDP_VERSION=\"0.14.2\" -I. -I../../system -I/usr/X11R6/include  -I../include -I../../include   -DPDP_TARGET=linux -o v4l.o -c v4l.c
# v4l.c: In function 'v4l2_check_controls':
# v4l.c:767:47: error: 'V4L2_CID_HCENTER' undeclared (first use in this function); did you mean 'V4L2_CID_ROTATE'?
#   767 | #define ZL_V4L_CTRL(id) v4l2_check_control(x, V4L2_CID_##id);
#
# - find upstream build fixes?



fetch:
{ gcc, bash, coreutils, which,
  autoconf, pkg-config,
  puredata, gsl,
  fetchFromGitHub, fetchurl, stdenv, lib }:

let
  fetched = fetch { fetchurl = fetchurl; };
in

stdenv.mkDerivation rec {
  name = "pd-creb";
  version = "current";
  buildInputs = [
    gcc bash coreutils which
    autoconf pkg-config
    puredata gsl
  ];
  src = fetched.creb;
  pd = puredata;
  builder = ./builder.sh;
}
