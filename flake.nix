# A Nix Flake wrapper for libopencm3, specialized to uc_tools
# STM32F103 development.
{
  description = "synth & effects firmware based on uc_tools, rs_tools";
  inputs = {
    # Same unstable snapshot as used by /etc/net
    nixpkgs.url = "github:NixOS/nixpkgs/85f1ba3e51676fa8cc604a3d863d729026a6b8eb";
    # nixpkgs.url = github:zwizwa/nixpkgs;
    flake-utils.url = "github:numtide/flake-utils";
    flake-utils.inputs.nixpkgs.follows = "nixpkgs";
    libopencm3.url = github:zwizwa/libopencm3-flake;
    rust-overlay = {
      url = "github:oxalica/rust-overlay";
      inputs = {
        nixpkgs.follows = "nixpkgs";
        flake-utils.follows = "flake-utils";
      };
    };
    # Think of rs_tools as an evolution of functionality contained in
    # uc_tools: a collection of routines for bare metal
    # microcontroller development and host support.  Note however that
    # rs_tools is built separately as a static library using Cargo,
    # while uc_tools is included as source and contains the build
    # system for C code contained in synth_tools.  There are a couple
    # of different ways to organize the Rust+C combo and I am not yet
    # clear on how to streamline.
    rs_tools = {
      url = "git+file:///i/exo/rs_tools";
    };
    uc_tools = {
      type = "github";
      owner = "zwizwa";
      repo = "uc_tools";
      rev = "c0853b29811c5d184d39b630b3c848a86d5d5e9e";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, flake-utils, rust-overlay,
              uc_tools, libopencm3, rs_tools, }:
    flake-utils.lib.eachDefaultSystem (system:
      let pkgs = import nixpkgs {
            inherit system; 
            overlays = [ (import rust-overlay) ];
          };
          targets = [ 
            "thumbv6m-none-eabi"
            "thumbv7m-none-eabi"
            "thumbv7em-none-eabihf"
          ];
          # rust_version = "latest";
          rust_version = "1.75.0";
          rustToolchain = pkgs.rust-bin.stable.${rust_version}.default.override {
            inherit targets;
          };
          rustPlatform = pkgs.recurseIntoAttrs (pkgs.makeRustPlatform {
            rustc = rustToolchain;
            cargo = rustToolchain;
          });
          buildInputs = with pkgs; [
            gcc jack2 a2jmidid alsa-lib puredata sqlite
            gcc-arm-embedded 
            rustToolchain
            pkg-config
          ];

        rs_crate = target: # See also rs_tools/flake.nix
          let src = ./rs;
              pname = "synth_tools_rs";
          in rustPlatform.buildRustPackage {
            inherit src pname;
            version = "rs_tools";
            auditable = false;
            cargoLock = {
              lockFile = src + "/Cargo.lock";
            };
            buildInputs = with pkgs; [ ];
            buildPhase = ''
              cargo build --release --target ${target}
            '';
            doCheck = false;
            installPhase = ''
              mkdir -p $out
              find -name '*.a'
              cp `find -name lib${pname}.a` $out/
            '';
          };
        rs_lib = target: "${rs_crate target}/libsynth_tools_rs.a";

      in
        {
          defaultPackage =
            pkgs.stdenv.mkDerivation {
              name = "synth_tools";
              inherit buildInputs;
              RS_A_HOST = rs_lib "x86_64-unknown-linux-gnu";
              RS_A_STM  = rs_lib "thumbv6m-none-eabi";
              src = self;
              inherit uc_tools; # Source
              TPF = "arm-none-eabi-";
              LIBOPENCM3 = libopencm3.packages.${system}.default; # Compiled
              enableParallelBuilding = true;
              builder = ./builder.sh;
            };

          devShells.default =
            pkgs.mkShell {
              packages = buildInputs;
              TPF = "${pkgs.gcc-arm-embedded}/bin/arm-none-eabi-";
              LIBOPENCM3 = libopencm3.packages.${system}.default;
              shellHook = ''
                PS1="(synth_tools) \u@\h:\w\$ "
              '';
            };
        }
    );
}
