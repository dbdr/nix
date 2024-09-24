{ lib, nixpkgs, nixpkgsFor }:

let

  nixos-lib = import (nixpkgs + "/nixos/lib") { };

  # https://nixos.org/manual/nixos/unstable/index.html#sec-calling-nixos-tests
  runNixOSTestFor = system: test: nixos-lib.runTest {
    imports = [ test ];
    hostPkgs = nixpkgsFor.${system}.native;
    defaults = {
      nixpkgs.pkgs = nixpkgsFor.${system}.native;
    };
    _module.args.nixpkgs = nixpkgs;
  };

in

{
  authorization = runNixOSTestFor "x86_64-linux" ./authorization.nix;

  remoteBuilds = runNixOSTestFor "x86_64-linux" ./remote-builds.nix;

  remoteBuildsSshNg = runNixOSTestFor "x86_64-linux" ./remote-builds-ssh-ng.nix;

  nix-copy-closure = runNixOSTestFor "x86_64-linux" ./nix-copy-closure.nix;

  nix-copy = runNixOSTestFor "x86_64-linux" ./nix-copy.nix;

  nssPreload = runNixOSTestFor "x86_64-linux" ./nss-preload.nix;

  githubFlakes = runNixOSTestFor "x86_64-linux" ./github-flakes.nix;

  sourcehutFlakes = runNixOSTestFor "x86_64-linux" ./sourcehut-flakes.nix;

  tarballFlakes = runNixOSTestFor "x86_64-linux" ./tarball-flakes.nix;

  containers = runNixOSTestFor "x86_64-linux" ./containers/containers.nix;

  setuid = lib.genAttrs
    ["i686-linux" "x86_64-linux"]
    (system: runNixOSTestFor system ./setuid.nix);

  ca-fd-leak = runNixOSTestFor "x86_64-linux" ./ca-fd-leak;

  user-sandboxing = runNixOSTestFor "x86_64-linux" ./user-sandboxing;
<<<<<<< HEAD
=======

  s3-binary-cache-store = runNixOSTestFor "x86_64-linux" ./s3-binary-cache-store.nix;

  fsync = runNixOSTestFor "x86_64-linux" ./fsync.nix;

  cgroups = runNixOSTestFor "x86_64-linux" ./cgroups;

  fetchurl = runNixOSTestFor "x86_64-linux" ./fetchurl.nix;
>>>>>>> f2f47fa72 (Add a test for builtin:fetchurl cert verification)
}
