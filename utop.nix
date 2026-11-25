with import <nixpkgs> {};
let version = "0.12.0"; in stdenv.mkDerivation {
  name = "utop-${version}";
  version = version;

  src = ./.;

  nativeBuildInputs = [
    ed
  ];
  buildInputs = [
    ncurses
  ];

  configurePhase = ''
    ./configure
  '';

  buildPhase = ''
    make
  '';

  installPhase = ''
    make PREFIX=$out install
  '';
}
