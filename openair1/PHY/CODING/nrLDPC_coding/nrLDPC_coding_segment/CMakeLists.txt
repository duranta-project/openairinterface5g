# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(ldpc_segment OBJECT
  nr_rate_matching.c
  nrLDPC_coding_segment_decoder.c
  nrLDPC_coding_segment_encoder.c
)
target_link_libraries(ldpc_segment PUBLIC log_headers)

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
