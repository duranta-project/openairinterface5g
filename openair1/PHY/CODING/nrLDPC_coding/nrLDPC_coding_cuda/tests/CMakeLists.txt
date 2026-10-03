# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_ldpc_cuda_unpack_output test_ldpc_cuda_unpack_output.cpp)
target_link_libraries(test_ldpc_cuda_unpack_output PRIVATE GTest::gtest)
add_dependencies(tests test_ldpc_cuda_unpack_output)
add_test(NAME test_ldpc_cuda_unpack_output
  COMMAND ./test_ldpc_cuda_unpack_output)
