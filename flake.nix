{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }: let
    forEachSystem = nixpkgs.lib.genAttrs [ "x86_64-linux" "aarch64-linux" ];
  in {
    devShells = forEachSystem (system: let pkgs = nixpkgs.legacyPackages.${system}; in {
      default = pkgs.mkShell {
        nativeBuildInputs = [
          pkgs.gdb
        ];
      };
    });
  };
}
