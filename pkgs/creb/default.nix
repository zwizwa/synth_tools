let fetch = { fetchurl }: {
creb = fetchurl {
  url = https://github.com/zwizwa/creb/archive/exo.tar.gz;
  sha256 = "10c53xllbmi8y7alf632zxfzsfrdmaliy4lfdxyy7bdpfdkk7jwl";
};
};
in import ./generic.nix fetch
