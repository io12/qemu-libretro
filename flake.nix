{
  description = "Build qemu-libretro the way libretro's CI does, and test the cores";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs {
        inherit system;
        # The Android SDK, emulator and system images are unfree and need
        # their license accepted to be fetched.
        config = {
          allowUnfree = true;
          android_sdk.accept_license = true;
        };
      };
      inherit (pkgs) lib;

      # libretro's build images (libretro-infrastructure/libretro-build-*),
      # pinned to the Dockerfiles behind the tags ci-templates uses
      dockerfile =
        repo: file: rev: hash:
        pkgs.fetchurl {
          name = "${repo}-${file}";
          url = "https://git.libretro.com/api/v4/projects/libretro-infrastructure%2Flibretro-build-${repo}/repository/files/${file}/raw?ref=${rev}";
          inherit hash;
        };
      dockerfiles = {
        mxe =
          dockerfile "mxe-win-cross-cores" "Dockerfile.gcc16" "b9fb84361f512ae6210e2306ac998d2069393ce0"
            "sha256-S/M5ZVJEDo2EpXoxV9ZS3Q4HX/DeOI3FKzr4NrP7QEY=";
        android =
          dockerfile "android" "Dockerfile" "ced9a8eb0c5be4217c3239645090ed3abacbb143"
            "sha256-O1dIICfT2Q9M7u9QmdFljiAt1luIPAA+c2HL1f6fuko=";
        amd64 =
          dockerfile "amd64-ubuntu" "Dockerfile.xenial-gcc9" "b238311abeb3f82745164186f49da5b94b075d18"
            "sha256-sNrDBdWbcc4SGQwXrYYa5xaODRU0l/LZEolE3nX6VTk=";
        i386 =
          dockerfile "i386-ubuntu" "Dockerfile.xenial-gcc9" "1db8057674a912ac23874ff32abd0a8f4e2d7889"
            "sha256-WySxpvhz6vPN2Hl2n3tHaA4L1/kc1xAmwz/0cqUfh9c=";
      };

      # Flakes don't include submodules, so fetch libretro.h at the commit
      # the libretro-common submodule is pinned to
      libretroHeader = pkgs.fetchurl {
        url = "https://raw.githubusercontent.com/libretro/libretro-common/70ed90c42ddea828f53dd1b984c6443ddb39dbd6/include/libretro.h";
        hash = "sha256-pQi3Xl3t3QEnK7cCt6UiOa/v5QuGH1ROtk6xHpH6tfs=";
      };

      android = pkgs.androidenv.composeAndroidPackages {
        platformVersions = [ "30" ];
        includeEmulator = true;
        includeSystemImages = true;
        # x86_64 images also run x86 binaries
        systemImageTypes = [ "default" ];
        abiVersions = [ "x86_64" ];
        includeNDK = true;
      };
      androidSdk = "${android.androidsdk}/libexec/android-sdk";
      ndkClang = "${androidSdk}/ndk-bundle/toolchains/llvm/prebuilt/linux-x86_64/bin/clang";

      # bionic and /system from Google's Android 11 (API 30, the cores'
      # minimum) arm64 GSI, for running ARM Android binaries under qemu-user;
      # the emulator can't run ARM images on x86 hosts. Unlike the emulator's
      # arm64 images, this GSI also has 32-bit ARM libraries. The libc and
      # linker are in the runtime APEX.
      androidSysrootArm =
        pkgs.runCommand "android-11-gsi-arm-sysroot"
          {
            src = pkgs.fetchurl {
              url = "https://dl.google.com/developers/android/rvc/images/gsi/aosp_arm64-exp-RP1A.200720.009-6720564-019b517d.zip";
              hash = "sha256-AZtRfa1MPuR1MCtOjpb+NZNJaKCDxO3+OHYR8I4vcbw=";
            };
            nativeBuildInputs = [
              pkgs.e2fsprogs
              pkgs.unzip
            ];
          }
          ''
            unzip -q $src system.img
            mkdir -p $out/apex/com.android.runtime
            debugfs -R "rdump /system $out" system.img
            debugfs -R "dump /system/apex/com.android.runtime.apex runtime.apex" system.img
            unzip -q runtime.apex apex_payload.img
            debugfs -R "rdump / $out/apex/com.android.runtime" apex_payload.img
            # Point absolute symlinks (/system/bin/linker -> /apex/...) into
            # the sysroot, since the host resolves them, not qemu-user
            find $out -type l | while read -r link; do
              target=$(readlink "$link")
              case $target in /*) ln -sfn "$out$target" "$link" ;; esac
            done
            test -e $out/system/bin/linker
            test -e $out/system/bin/linker64
          '';

      # Only the ARM targets, with a fix for running 32-bit bionic under
      # seccomp filters like Docker's (see the patch)
      qemuUser =
        (pkgs.qemu-user.override {
          hostCpuTargets = [
            "arm-linux-user"
            "aarch64-linux-user"
          ];
        }).overrideAttrs
          (old: {
            patches = old.patches or [ ] ++ [ ./tests/libretro/qemu-user-personality.patch ];
          });

      frontendSrc = ./tests/libretro/frontend.c;

      # The test frontend, built for each platform a core is tested on
      buildFrontend =
        name: cc: flags:
        let
          exe = if lib.hasPrefix "win" name then "frontend.exe" else "frontend";
        in
        pkgs.runCommand "qemu-libretro-test-frontend-${name}" { meta.mainProgram = exe; } ''
          mkdir -p include $out/bin
          cp ${libretroHeader} include/libretro.h
          ${cc} -O2 -Wall -Werror -Iinclude ${frontendSrc} -o $out/bin/${exe} ${flags}
        '';
      mingwCC =
        p:
        "${p.stdenv.cc}/bin/${p.stdenv.cc.targetPrefix}cc -L${p.windows.mcfgthreads}/lib";
      androidCC = target: "${ndkClang} --target=${target}30";
      frontends = {
        linux-x64 = buildFrontend "linux-x64" "${pkgs.stdenv.cc}/bin/cc" "-ldl";
        linux-i686 = buildFrontend "linux-i686" "${pkgs.pkgsi686Linux.stdenv.cc}/bin/cc" "-ldl";
        win64 = buildFrontend "win64" (mingwCC pkgs.pkgsCross.mingwW64) "-static";
        win32 = buildFrontend "win32" (mingwCC pkgs.pkgsCross.mingw32) "-static";
        android-x86_64 = buildFrontend "android-x86_64" (androidCC "x86_64-linux-android") "";
        android-x86 = buildFrontend "android-x86" (androidCC "i686-linux-android") "";
        android-arm64 = buildFrontend "android-arm64" (androidCC "aarch64-linux-android") "";
        android-arm = buildFrontend "android-arm" (androidCC "armv7a-linux-androideabi") "";
      };

      # Boot sector test guest, raw and as a zstd-compressed qcow2 (issue #7)
      testGuest =
        pkgs.runCommand "qemu-libretro-test-guest"
          {
            nativeBuildInputs = [
              pkgs.nasm
              pkgs.qemu-utils
            ];
          }
          ''
            mkdir $out
            nasm -f bin -o $out/guest.img ${./tests/libretro/guest.asm}
            qemu-img convert -f raw -O qcow2 -c -o compression_type=zstd \
              $out/guest.img $out/guest-zstd.qcow2
          '';

      ci = pkgs.writeShellScriptBin "qemu-libretro-ci" ''
        export PATH=${
          lib.makeBinPath [
            pkgs.buildah
            pkgs.util-linux
            pkgs.coreutils
            pkgs.gnutar
            pkgs.gawk
            pkgs.gnused
            pkgs.git
          ]
        }:$PATH
        export CI_POLICY=${pkgs.writeText "policy.json" ''{"default":[{"type":"insecureAcceptAnything"}]}''}
        # The Dockerfiles use short names like ubuntu:noble
        export CONTAINERS_REGISTRIES_CONF=${pkgs.writeText "registries.conf" ''unqualified-search-registries = ["docker.io"]''}
        ${lib.concatStrings (
          lib.mapAttrsToList (name: file: "export CI_DOCKERFILE_${name}=${file}\n") dockerfiles
        )}
        exec ${pkgs.bash}/bin/bash ${./ci/ci.sh} "$@"
      '';

      test = pkgs.writeShellScriptBin "qemu-libretro-test" ''
        export PATH=${
          lib.makeBinPath [
            pkgs.wineWow64Packages.stable
            qemuUser
            pkgs.util-linux
            pkgs.coreutils
            pkgs.gnugrep
            pkgs.gnused
            pkgs.gawk
            pkgs.patchelf
            pkgs.git
            android.androidsdk
          ]
        }:$PATH
        export ANDROID_SDK_ROOT=${androidSdk} ANDROID_HOME=${androidSdk}
        export ANDROID_SYSTEM_IMAGE="system-images;android-30;default;x86_64"
        export TEST_GUEST_DIR=${testGuest}
        export ANDROID_SYSROOT_ARM=${androidSysrootArm}
        ${lib.concatStrings (
          lib.mapAttrsToList (
            name: drv: "export FRONTEND_${lib.replaceStrings [ "-" ] [ "_" ] name}=${lib.getExe drv}\n"
          ) frontends
        )}
        exec ${pkgs.bash}/bin/bash ${./tests/libretro/run-tests.sh} "$@"
      '';
    in
    {
      packages.${system} = {
        inherit
          ci
          test
          testGuest
          androidSysrootArm
          ;
        default = ci;
      }
      // lib.mapAttrs' (name: drv: lib.nameValuePair "frontend-${name}" drv) frontends;

      apps.${system} = {
        ci = {
          type = "app";
          program = "${ci}/bin/qemu-libretro-ci";
        };
        test = {
          type = "app";
          program = "${test}/bin/qemu-libretro-test";
        };
      };
    };
}
