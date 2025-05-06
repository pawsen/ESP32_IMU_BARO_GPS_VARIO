# use with
# nix develop -c $SHELL
{
  description = "nix flake for platformIO";
  inputs = { nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable"; };
  outputs = { self, nixpkgs }:
    let
      # Supported systems
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" ];

      # Helper to generate system-specific attributes
      forAllSystems = f:
        nixpkgs.lib.genAttrs systems
        (system: f { pkgs = import nixpkgs { inherit system; }; });

    in {
      devShells = forAllSystems ({ pkgs }: {
        default = pkgs.mkShell {
          name = "platformIO-dev";
          # The Nix packages provided in the environment
          packages = with pkgs; [
            platformio
            esptool  # should be needed for ESP flashing
            python3
            gcc
            gnumake
            udev
            zlib
            stdenv.cc.cc.lib
            glibc
            libusb1
            ncurses  # Required for terminal tools
            # use tio instead of screen, tio -b 9600 /dev/ttyUSB0
            tio
            openocd # Optional debugging
          ];

          # buildInputs = with pkgs; [];

          # For adding binaries to PATH:
          # nativeBuildInputs = [ pkgs.python3 ]

          # Set any environment variables for your dev shell
          # Can only use Nix store paths (no runtime paths like $PWD) and are
          # visible in nix print-dev-env
          env = {
            # include toolchain headers when creating compile_commands.json
            # https://github.com/platformio/platformio-core/issues/4130
            COMPILATIONDB_INCLUDE_TOOLCHAIN="True";
          };

          # Add any shell logic you want executed any time the environment is activated
          shellHook = ''
            # All platform development package are saved in $PLATFORMIO_CORE_DIR
            # export PLATFORMIO_CORE_DIR=$PWD/.platformio
            export PLATFORMIO_CORE_DIR="''${PLATFORMIO_CORE_DIR:-$HOME/.platformio}"
            # ${pkgs.platformio}/lib/python3.12/site-packages does not exist
            # export PYTHONPATH="${pkgs.platformio}/lib/python3.12/site-packages:$PYTHONPATH"
            echo "Arduino development shell ready"
            echo "PlatformIO dir: $PLATFORMIO_CORE_DIR"
            echo "Run 'pio run' to build"

            # https://docs.platformio.org/en/latest/integration/compile_commands.html
            echo "Run 'pio run -t compiledb' to generate compile_commands.json for clangd"
          '';

        };
      });
    };
}
