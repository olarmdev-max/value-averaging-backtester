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
        pythonEnv = pkgs.python3.withPackages (ps: [ ps.yfinance ]);
      in {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "cpp-backtester";
          version = "0.1.0";
          src = self;

          nativeBuildInputs = with pkgs; [ cmake ninja pkg-config ];
          buildInputs = with pkgs; [ dlib eigen python3 ];

          cmakeFlags = [ "-DCPP_BACKTESTER_ENABLE_DLIB=ON" ];
        };

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            ninja
            pkg-config
            gcc
            gdb
            dlib
            eigen
            clang-tools
            pythonEnv
          ];

          shellHook = ''
            export CMAKE_EXPORT_COMPILE_COMMANDS=1
            echo "cpp-backtester dev shell loaded"
            echo "dlib available through nix shell"
            echo "python has yfinance available in this shell"
          '';
        };
      });
}
