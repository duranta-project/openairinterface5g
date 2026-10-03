# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_subdirectory(nrLDPC_coding_segment)
add_subdirectory(nrLDPC_coding_aal)
add_boolean_option(ENABLE_LDPC_CUDA OFF "Build support for CUDA" OFF)
if (ENABLE_LDPC_CUDA)
  add_subdirectory(nrLDPC_coding_cuda)
endif()
# host-only parts of the CUDA library, tested regardless of ENABLE_LDPC_CUDA
if (ENABLE_TESTS)
  add_subdirectory(nrLDPC_coding_cuda/tests)
endif()
add_boolean_option(ENABLE_LDPC_ORS OFF "Build support for OpenRadioSystems Aurora LDPC accelerator" OFF)
if (ENABLE_LDPC_ORS)
  add_subdirectory(nrLDPC_coding_ors)
endif()
