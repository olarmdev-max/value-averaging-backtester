{
  description = "C++ backtester with Nix dev shell and dlib-ready optimization support";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "cpp-backtester";
          version = "0.1.0";
          src = self;

          nativeBuildInputs = with pkgs; [ cmake ninja pkg-config ];
          buildInputs = with pkgs; [ dlib eigen python3 ];

          cmakeFlags = [ "-DCPP_BACKTESTER_ENABLE_DLIB=ON" ];

          buildPhase = ''
            runHook preBuild
            cmake -S . -B build -G Ninja $cmakeFlags
            cmake --build build -j2
            runHook postBuild
          '';

          installPhase = ''
            runHook preInstall
            mkdir -p $out/bin
            cp build/cpp_backtester $out/bin/
            runHook postInstall
          '';
        };

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            ninja
            pkg-config
            gcc
            gdb
            python3
            dlib
            eigen
            clang-tools
          ];

          shellHook = ''
            export CMAKE_EXPORT_COMPILE_COMMANDS=1
            echo "cpp-backtester dev shell loaded"
            echo "dlib available through nix shell"
          '';
        };
      });
}
