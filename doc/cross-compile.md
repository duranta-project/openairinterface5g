<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# OpenAirInterface Cross-Compiler User Guide

This document explains how to build OAI for ARM64
(using the instruction set aarch64) and RISC-V.

[[_TOC_]]

## Environment

Tested on Ubuntu 22. Newer version of Ubuntu should work as well, please file a
bug report if not (Gitlab Issues page).

You should be able to compile OAI on the host (i.e., for x86). To do so,
install the dependencies, if not done already:

```shell
cmake_targets/build_oai -I
```

## Install ARM64 dependencies

Set up for install the package for ARM64.

```shell
sudo dpkg --add-architecture arm64

echo -e \
        "deb [arch=arm64] http://ports.ubuntu.com/ jammy main restricted\n"\
        "deb [arch=arm64] http://ports.ubuntu.com/ jammy-updates main restricted\n"\
        "deb [arch=arm64] http://ports.ubuntu.com/ jammy universe\n"\
        "deb [arch=arm64] http://ports.ubuntu.com/ jammy-updates universe\n"\
        "deb [arch=arm64] http://ports.ubuntu.com/ jammy multiverse\n"\
        "deb [arch=arm64] http://ports.ubuntu.com/ jammy-updates multiverse\n"\
        "deb [arch=arm64] http://ports.ubuntu.com/ jammy-backports main restricted universe multiverse"\
    | sudo tee /etc/apt/sources.list.d/arm-cross-compile-sources.list

sudo cp /etc/apt/sources.list "/etc/apt/sources.list.`date`.backup"
sudo sed -i -E "s/(deb)\ (http:.+)/\1\ [arch=amd64]\ \2/" /etc/apt/sources.list

sudo apt update
sudo apt install -y gcc-aarch64-linux-gnu \
                    g++-aarch64-linux-gnu

sudo apt-get install -y \
    libc6-dev-i386 \
    libreadline-dev:arm64 \
    libgnutls28-dev:arm64 \
    libconfig-dev:arm64 \
    libsctp-dev:arm64 \
    libssl-dev:arm64 \
    libtool:arm64 \
    zlib1g-dev:arm64
```

The above enables apt to download packages for arm64. It also installs
gcc cross-compilers for ARM64 in version 11. This version needs to match the
versions of gcc defined in the cmake cross-compilation file (`cross-arm.cmake`).

## Build for ARM64

### Build code generation tools for host

Use the x86 compiler to generate the T header file in the `ran_build/build`
folder.  This is necessary during a build for code generation, and therefore
need to be created for the x86 architecture.

```shell
rm -r ran_build
mkdir ran_build
mkdir ran_build/build
mkdir ran_build/build-cross

cd ran_build/build
cmake ../../..
make -j`nproc` generate_T
```

### Build executables for ARM64

Switch to the `ran_build/build-cross` folder to build the target executables
for ARM. The `cross-arm.cmake` file defines some ARM-specific build tools
(e.g., compilers) that you might need to adapt. Further, it defines cmake
variables that define in this step where the host tools (such as LDPC
generators) are to be found. For the latter, the `NATIVE_DIR` option has to
be defined in order to tell cmake where the host tools have been built.

```shell
cd ../build-cross
cmake ../../.. -GNinja -DCMAKE_TOOLCHAIN_FILE=../../../cmake_targets/cross-arm.cmake -DNATIVE_DIR=../build

ninja dlsim ulsim ldpctest polartest smallblocktest nr_pbchsim nr_dlschsim nr_ulschsim nr_dlsim nr_ulsim nr_pucchsim nr_prachsim nr_srssim
ninja lte-softmodem nr-softmodem nr-cuup oairu lte-uesoftmodem nr-uesoftmodem
ninja params_libconfig coding rfsimulator
```

## Further information

You can do the above steps using docker, see dockerfiles
`docker/Dockerfile.base.ubuntu.cross-arm64` and
`docker/Dockerfile.build.ubuntu.cross-arm64` for more information.

## Install RISC-V dependencies

When using an existing target sysroot, the host still needs a RISC-V compiler
and pkg-config:

```shell
sudo apt install -y gcc-riscv64-linux-gnu g++-riscv64-linux-gnu pkg-config
```

## Build for RISC-V

The RISC-V flow is the same two-stage build as ARM64: first build the native
code-generation tools for the x86 host, then configure a cross build with a
RISC-V toolchain file.

The default RISC-V sysroot is `$HOME/sysroots/k3`. You can override it
with either `K3_SYSROOT`, `RISCV_SYSROOT`, or the CMake cache variable
`OAI_RISCV_SYSROOT`.

The default compiler prefix is `riscv64-linux-gnu`. Install a matching host
cross compiler, or override `OAI_RISCV_C_COMPILER` and
`OAI_RISCV_CXX_COMPILER` if your compiler is elsewhere. For the SpaceMIT K3
Linux glibc toolchain, use `-DOAI_RISCV_TOOLCHAIN_PREFIX=riscv64-unknown-linux-gnu`.

The default RISC-V CPU flags are `-march=rv64gc_zba_zbb_zbs_zicond`,
`-mabi=lp64d`, and `-mtune=generic-ooo`. Override `OAI_RISCV_MARCH`,
`OAI_RISCV_MABI`, or `OAI_RISCV_MTUNE` if the K3 compiler/runtime needs a
more specific ISA string.

```shell
rm -r ran_build
mkdir ran_build
mkdir ran_build/build
mkdir ran_build/build-riscv

cd ran_build/build
cmake ../../..
make -j`nproc` generate_T

cd ../build-riscv
cmake ../../.. -GNinja -DCMAKE_TOOLCHAIN_FILE=../../../cmake_targets/cross-riscv.cmake -DNATIVE_DIR=../build

ninja dlsim ulsim ldpctest polartest smallblocktest nr_pbchsim nr_dlschsim nr_ulschsim nr_dlsim nr_ulsim nr_pucchsim nr_prachsim nr_srssim
ninja nr-softmodem nr-uesoftmodem
ninja params_libconfig rfsimulator
# dlopen'd runtime plugins (MODULE libs). These are NOT link-time deps of the
# sims/softmodem, so the targets above do NOT build them -- build them
# explicitly and keep them on LD_LIBRARY_PATH where the binaries run, or PHY
# init fails at runtime ("error loading LDPC library", DFT not found):
ninja dfts ldpc ldpc_orig coding
```

Example with explicit paths:

```shell
cmake ../../.. -GNinja \
  -DCMAKE_TOOLCHAIN_FILE=../../../cmake_targets/cross-riscv.cmake \
  -DNATIVE_DIR=../build \
  -DOAI_RISCV_SYSROOT=$HOME/sysroots/k3 \
  -DOAI_RISCV_C_COMPILER=/opt/riscv/bin/riscv64-linux-gnu-gcc \
  -DOAI_RISCV_CXX_COMPILER=/opt/riscv/bin/riscv64-linux-gnu-g++
```

### Enabling the RVV (vector) SIMD path

The default `OAI_RISCV_MARCH` (`rv64gc_zba_zbb_zbs_zicond`) does **not** include
the `v` (vector) extension. The hand-written RISC-V Vector (RVV) kernels in the
NR PHY are gated on `__riscv_vector`, so with the default march they compile out
and the build silently falls back to the (scalar) SIMDe emulation — correct, but
much slower.

To build the accelerated path, add the vector extension (and `zbc`, for the
carry-less-multiply CRC) to the march string. The following string is validated
on the SpaceMIT K3:

```shell
cmake ../../.. -GNinja \
  -DCMAKE_TOOLCHAIN_FILE=../../../cmake_targets/cross-riscv.cmake \
  -DNATIVE_DIR=../build \
  -DOAI_RISCV_MARCH=rv64gcv_zba_zbb_zbc_zbs_zicond
```

The RVV kernels are vector-length-agnostic (one VLA implementation), so a single
binary runs unchanged on any VLEN (e.g. 256-bit and 1024-bit cores); no
minimum-VLEN pinning is required. Changing `OAI_RISCV_MARCH` on an existing build
directory requires a fresh `cmake` configure (reconfigure), not just a rebuild.

## Notes for other host distributions / CI

The dependency-install commands above are Ubuntu/Debian-specific (`apt`,
`:arm64` multiarch). On RHEL/OpenShift (OCP) or other distros, install the
equivalent RISC-V cross toolchain and a target sysroot via the distro package
manager or a build container (see the ARM `docker/Dockerfile.*.cross-arm64`
files as a template). The toolchain-file variables themselves
(`OAI_RISCV_C_COMPILER`, `OAI_RISCV_CXX_COMPILER`, `OAI_RISCV_SYSROOT`,
`OAI_RISCV_MARCH`) are distribution-independent, so only the dependency
bootstrap differs. `ccache` is honored by the build and is recommended for CI to
avoid repaying the one long compile (`oai_dfts_rvv.c`, the mixed-radix DFT).
