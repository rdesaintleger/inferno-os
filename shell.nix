{ pkgs ? import <nixpkgs> { system = "i686-linux"; } }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    gdb
    xorg.libX11
    xorg.libXext
    xorg.libXpm
  ];
}
