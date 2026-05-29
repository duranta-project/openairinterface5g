# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_subdirectory(nrLDPC_coding_segment)
add_subdirectory(nrLDPC_coding_aal)
add_boolean_option(ENABLE_LDPC_CUDA OFF "Build support for CUDA" OFF)
