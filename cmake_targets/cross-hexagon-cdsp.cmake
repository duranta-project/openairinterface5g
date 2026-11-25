# SPDX-License-Identifier: LicenseRef-CSSL-1.0
# CMake toolchain for code running on a Hexagon compute DSP (FastRPC skel
# libraries), using hexagon-clang from the Hexagon SDK. HEXAGON_TOOLS_ROOT is the
# toolchain directory found by FindHexagonSDK (HexagonSDK_TOOLS_ROOT).

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR hexagon)

list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES HEXAGON_TOOLS_ROOT)
set(CMAKE_C_COMPILER "${HEXAGON_TOOLS_ROOT}/bin/hexagon-clang")
# there is no standalone executable to link for a skel library
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
