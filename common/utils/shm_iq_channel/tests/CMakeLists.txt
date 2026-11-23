# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_shm_td_iq_channel test_shm_td_iq_channel.c)
target_link_libraries(test_shm_td_iq_channel shm_td_iq_channel minimal_lib pthread)
add_dependencies(tests test_shm_td_iq_channel)
# Commented out due to issues with ASAN in the unit test docker container
#add_test(test_shm_td_iq_channel ./test_shm_td_iq_channel)

add_executable(test_shm_fd_iq_channel test_shm_fd_iq_channel.cpp)
target_link_libraries(test_shm_fd_iq_channel PRIVATE shm_fd_iq_channel minimal_lib GTest::gtest)
add_dependencies(tests test_shm_fd_iq_channel)
add_test(NAME test_shm_fd_iq_channel COMMAND ./test_shm_fd_iq_channel)
