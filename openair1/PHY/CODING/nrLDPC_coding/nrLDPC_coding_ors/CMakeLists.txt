# SPDX-License-Identifier: LicenseRef-CSSL-1.0

##########################################################

# LDPC offload library - XDMA
##########################################################

add_library(ldpc_ors MODULE
            nrLDPC_coding_xdma_offload.c
            nrLDPC_coding_xdma.c
            ../nrLDPC_coding_segment/nr_rate_matching.c
            ../nrLDPC_coding_segment/nrLDPC_coding_segment_encoder.c
            ../../nrLDPC_encoder/ldpc_encoder_optim8segmulti.c
)
target_include_directories(ldpc_ors PRIVATE ../nrLDPC_coding_segment)
target_link_libraries(ldpc_ors PRIVATE log_headers)
set_target_properties(ldpc_ors PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_dependencies(nr-softmodem ldpc_ors)
add_dependencies(nr-uesoftmodem ldpc_ors)
add_dependencies(nr_ulsim ldpc_ors)
add_dependencies(nr_ulschsim ldpc_ors)
add_dependencies(nr_dlsim ldpc_ors)
add_dependencies(nr_dlschsim ldpc_ors)
