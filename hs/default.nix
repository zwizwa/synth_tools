{ mkDerivation, base, binary, bytestring, command, containers
, data-fix, data-reify, directory, fft, lens, lib, mtl
, pretty-simple, process, QuickCheck, split, Stream
}:
mkDerivation {
  pname = "synth-tools";
  version = "1.0.0";
  src = ./.;
  isLibrary = false;
  isExecutable = true;
  executableHaskellDepends = [
    base binary bytestring command containers data-fix data-reify
    directory fft lens mtl pretty-simple process QuickCheck split
    Stream
  ];
  license = lib.licenses.bsd3;
}
