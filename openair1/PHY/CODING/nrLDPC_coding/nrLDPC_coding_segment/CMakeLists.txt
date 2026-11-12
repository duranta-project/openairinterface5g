# SPDX-License-Identifier: LicenseRef-CSSL-1.0

# Rate matching and the segment encoder, shared by the CPU decoder libraries
# and the offload libraries that bring their own decoder entry points.
add_library(ldpc_segment_common OBJECT
  nr_rate_matching.c
  nrLDPC_coding_segment_encoder.c
)
target_include_directories(ldpc_segment_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ldpc_segment_common PUBLIC log_headers)

# The segment decoder (nrLDPC_coding_init/decoder/shutdown on the CPU).
# Object files are only linked into targets that link an object library
# directly, so users link ldpc_segment_common as well.
add_library(ldpc_segment OBJECT
  nrLDPC_coding_segment_decoder.c
)
target_link_libraries(ldpc_segment PUBLIC ldpc_segment_common)

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
