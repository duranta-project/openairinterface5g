# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_ldpc_unpack_output_cuda32 test_ldpc_unpack_output_cuda32.cpp ../ldpc_unpack_output_cuda32.c)
target_link_libraries(test_ldpc_unpack_output_cuda32 PRIVATE GTest::gtest)
add_dependencies(tests test_ldpc_unpack_output_cuda32)
add_test(NAME test_ldpc_unpack_output_cuda32
  COMMAND ./test_ldpc_unpack_output_cuda32)
