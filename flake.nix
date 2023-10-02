{
  description = "Kernel, Zig dev";

  inputs = {
    pkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    zig = {
      url = "github:mitchellh/zig-overlay";
      inputs.nixpkgs.follows = "pkgs";
    };
    zls = {
      url = "github:zigtools/zls";
      inputs.zig-overlay.follows = "zig";
    };
  };

  outputs = { self, pkgs, zig, zls }:
    let
      sys = "x86_64-linux";
    in
      with import pkgs { system = "${sys}"; config.allowUnfree = true; };
      {
        devShells.${sys}.default = mkShell {
          buildInputs = [
            zig.packages.${sys}.master
            zls.packages.${sys}.zls

            cmake

            pwndbg
          ];
        };
      };
}
