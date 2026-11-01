{ pkgs ? import <nixpkgs> { system = "i686-linux"; } }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    gdb
    xorg.libX11
    xorg.libXext
    xorg.libXpm
  ];

  shellHook = ''
    export PATH="$PWD/Linux/386/bin:$PATH"
  '';
}