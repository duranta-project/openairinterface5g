# SPDX-License-Identifier: LicenseRef-CSSL-1.0
# CMake toolchain for the ARM side of the Qualcomm DragonWing IQ-9/X (QCS9075,
# Kryo Gold = Cortex-A78), using the aarch64 GCC shipped in the Hexagon SDK.
# See doc/cross-compile.md, section "Qualcomm DragonWing IQ-9/X".

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(HEXAGON_SDK_ROOT "/opt/Hexagon_SDK/6.4.0.2" CACHE PATH "Hexagon SDK root (provides the ARM cross compiler)")
set(_dw_tc "${HEXAGON_SDK_ROOT}/tools/gcc_tools_64/bin/aarch64-none-linux-gnu")
set(CMAKE_C_COMPILER   "${_dw_tc}-gcc")
set(CMAKE_CXX_COMPILER "${_dw_tc}-g++")
set(CMAKE_AR           "${_dw_tc}-ar")

# +crypto defines __ARM_FEATURE_AES, which SIMDE needs to map the CRC's
# carry-less multiply onto PMULL; without it crc_byte.c uses a scalar emulation.
set(_dw_cpu "-mcpu=cortex-a78+crypto")

# The SDK GCC's own sysroot ships an incomplete glibc (stdio.h lacks L_tmpnam
# and others), so the Ubuntu arm64 multiarch headers and libraries are used.
set(CMAKE_C_FLAGS_INIT   "${_dw_cpu} -I/usr/include/aarch64-linux-gnu -I/usr/include")
set(CMAKE_CXX_FLAGS_INIT "${_dw_cpu} -I/usr/include/aarch64-linux-gnu -I/usr/include")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "-L/usr/lib/aarch64-linux-gnu")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-L/usr/lib/aarch64-linux-gnu")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-L/usr/lib/aarch64-linux-gnu")
set(CMAKE_LIBRARY_PATH "/usr/lib/aarch64-linux-gnu")
set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/aarch64-linux-gnu/pkgconfig")

set(CROSS_COMPILE 1)

# host-built code generators (NATIVE_DIR is relative to the build directory)
foreach(_gen bnProc_gen_128 bnProc_gen_avx2 bnProc_gen_avx512 cnProc_gen_128 cnProc_gen_avx2 cnProc_gen_avx512 genids _check_vcd)
  set(${_gen}_DIR ${CMAKE_CURRENT_BINARY_DIR}/${NATIVE_DIR})
endforeach()
