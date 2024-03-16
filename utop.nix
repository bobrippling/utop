with import <nixpkgs> {};
let version = "0.9.3"; in stdenv.mkDerivation {
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

# { stdenv }: stdenv.mkDerivation rec {
# }

# nix% nix-env -iA '"nixos-23.11".ncurses'
# nix% make CFLAGS=-I/nix/store/crv6<whatever>-ncurses-6.4-dev/include/
# ^ ncurses is found. alternatively (/properly), write this file, then:
#
# nix% nix-shell
# nix%% make
# ^ ncurses is found
