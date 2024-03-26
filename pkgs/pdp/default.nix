let fetch = { fetchurl }: {
pdp = fetchurl {
  url = https://github.com/zwizwa/pdp/archive/exo.tar.gz;
  sha256 = "1djil9d2zjf372g86x57d8j36ssjhzm85lb2x80gc31qhsknrfpx";
};
};
in import ./generic.nix fetch
