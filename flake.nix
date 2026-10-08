{
  description = "GPU fluid simulation (raylib + OpenGL 4.3 compute shaders)";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { nixpkgs, ... }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            git
            pkg-config
          ];

          buildInputs = with pkgs; [
            libglvnd
            libx11
            libxcursor
            libxext
            libxi
            libxinerama
            libxrandr
          ];
        };
      });
    };
}
