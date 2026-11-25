# SPDX-License-Identifier: LicenseRef-CSSL-1.0

# FindHexagonSDK
# --------------
#
# Finds the Qualcomm Hexagon SDK: the FastRPC host library and headers used by
# the ARM side of a DSP offload, the qaic IDL compiler, and the hexagon-clang
# toolchain that builds the DSP side.
#
# Required options
# ^^^^^^^^^^^^^^^^
#
# ``HEXAGON_SDK_ROOT``
#   The SDK installation, e.g. /opt/Hexagon_SDK/6.4.0.2. Also read from the
#   environment variable of the same name (set by the SDK's setup_sdk_env.source).
#
# Optional options
# ^^^^^^^^^^^^^^^^
#
# ``HEXAGON_DSP_ARCH``
#   Hexagon architecture of the DSP, e.g. v73 (the default). Selects the QuRT
#   headers of ``HexagonSDK::dsp``.
#
# Imported Targets
# ^^^^^^^^^^^^^^^^
#
# ``HexagonSDK::cdsprpc``
#   For code on the application processor: libcdsprpc, the FastRPC client
#   library for the compute DSP ("cDSP RPC"; the SoC's other DSPs have
#   libadsprpc and libsdsprpc), with the FastRPC headers. It loads the DSP
#   side of an offload and carries the remote calls and dspqueue packets.
# ``HexagonSDK::dsp``
#   For code built with hexagon-clang to run on the DSP: the SDK, FastRPC and
#   QuRT headers. Nothing to link; the toolchain provides the libraries.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# ``HexagonSDK_FOUND``
#   True if the SDK was found.
# ``HexagonSDK_INCLUDE_DIRS``
#   Headers for FastRPC host code (remote.h, rpcmem.h, dspqueue.h, ...).
# ``HexagonSDK_QAIC``
#   The qaic IDL compiler.
# ``HexagonSDK_TOOLS_ROOT``
#   The Hexagon toolchain (``bin/hexagon-clang``, ``target/hexagon/lib``).
#
# The cdsprpc library is the prebuilt one shipped in the SDK for Linux on
# aarch64 (ipc/fastrpc/remote/ship/UbuntuARM_aarch64).

if(NOT HEXAGON_SDK_ROOT AND DEFINED ENV{HEXAGON_SDK_ROOT})
  set(HEXAGON_SDK_ROOT "$ENV{HEXAGON_SDK_ROOT}")
endif()
set(HEXAGON_SDK_ROOT "${HEXAGON_SDK_ROOT}" CACHE PATH "Hexagon SDK root")

find_path(HexagonSDK_INCLUDE_DIR
  NAMES remote.h dspqueue.h
  HINTS ${HEXAGON_SDK_ROOT}
  PATH_SUFFIXES incs
  NO_DEFAULT_PATH
)
find_path(HexagonSDK_RPCMEM_INCLUDE_DIR
  NAMES rpcmem.h
  HINTS ${HEXAGON_SDK_ROOT}
  PATH_SUFFIXES ipc/fastrpc/rpcmem/inc
  NO_DEFAULT_PATH
)
find_library(HexagonSDK_CDSPRPC_LIBRARY
  NAMES cdsprpc
  HINTS ${HEXAGON_SDK_ROOT}
  PATH_SUFFIXES ipc/fastrpc/remote/ship/UbuntuARM_aarch64
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
)
find_program(HexagonSDK_QAIC
  NAMES qaic
  HINTS ${HEXAGON_SDK_ROOT}
  PATH_SUFFIXES ipc/fastrpc/qaic/bin
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
)
file(GLOB _hexagon_tools LIST_DIRECTORIES true "${HEXAGON_SDK_ROOT}/tools/HEXAGON_Tools/*/Tools")
list(SORT _hexagon_tools COMPARE NATURAL ORDER DESCENDING)
find_path(HexagonSDK_TOOLS_ROOT
  NAMES bin/hexagon-clang
  HINTS ${_hexagon_tools}
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
)
unset(_hexagon_tools)

string(REGEX MATCH "[0-9]+(\\.[0-9]+)+" HexagonSDK_VERSION "${HEXAGON_SDK_ROOT}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HexagonSDK
  FOUND_VAR HexagonSDK_FOUND
  REQUIRED_VARS
    HexagonSDK_INCLUDE_DIR
    HexagonSDK_RPCMEM_INCLUDE_DIR
    HexagonSDK_CDSPRPC_LIBRARY
    HexagonSDK_QAIC
    HexagonSDK_TOOLS_ROOT
  VERSION_VAR HexagonSDK_VERSION
)

if(HexagonSDK_FOUND)
  set(HexagonSDK_INCLUDE_DIRS
    ${HexagonSDK_INCLUDE_DIR}
    ${HexagonSDK_INCLUDE_DIR}/stddef
    ${HexagonSDK_RPCMEM_INCLUDE_DIR})
endif()

if(NOT HEXAGON_DSP_ARCH)
  set(HEXAGON_DSP_ARCH v73)
endif()
if(HexagonSDK_FOUND AND NOT TARGET HexagonSDK::dsp)
  set(_qurt ${HEXAGON_SDK_ROOT}/rtos/qurt/compute${HEXAGON_DSP_ARCH}/include)
  add_library(HexagonSDK::dsp INTERFACE IMPORTED)
  set_target_properties(HexagonSDK::dsp PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES
      "${HexagonSDK_INCLUDE_DIRS};${HEXAGON_SDK_ROOT}/ipc/fastrpc/incs;${_qurt};${_qurt}/qurt;${_qurt}/posix"
  )
  unset(_qurt)
endif()

if(HexagonSDK_FOUND AND NOT TARGET HexagonSDK::cdsprpc)
  add_library(HexagonSDK::cdsprpc UNKNOWN IMPORTED)
  set_target_properties(HexagonSDK::cdsprpc PROPERTIES
    IMPORTED_LOCATION "${HexagonSDK_CDSPRPC_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${HexagonSDK_INCLUDE_DIRS}"
  )
endif()

mark_as_advanced(
  HexagonSDK_INCLUDE_DIR
  HexagonSDK_RPCMEM_INCLUDE_DIR
  HexagonSDK_CDSPRPC_LIBRARY
  HexagonSDK_QAIC
  HexagonSDK_TOOLS_ROOT
)
