{ pkgs ? import <nixpkgs> { system = "i686-linux"; } }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    xorg.libX11
    xorg.libXext
    xorg.libXpm
  ];
}