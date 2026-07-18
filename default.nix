{
  stdenv,
  clang,
  glibc,
}:
stdenv.mkDerivation {
  pname = "eMC";
  version = "1.0";

  src = ./.;

  nativeBuildInputs = [clang];
  buildInputs = [glibc];

  buildPhase = let
    cpu = stdenv.hostPlatform.gcc.arch or "x86-64";
  in ''
    ${clang}/bin/clang -march=${cpu} -s -Wall -Wno-c23-extensions main.c -o eMC
  '';

  installPhase = ''
    mkdir -p $out/bin
    cp eMC $out/bin/eMC
  '';
}
