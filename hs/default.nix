{ mkDerivation, base, lib, Stream }:
mkDerivation {
  pname = "synth-tools";
  version = "1.0.0";
  src = ./.;
  isLibrary = false;
  isExecutable = true;
  executableHaskellDepends = [ base Stream ];
  license = lib.licenses.bsd3;
  ## FIXME: I had to comment this out.  Argument no longer supported?
  # mainProgram = "synth-tools";
}
