# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_vrtsim test_vrtsim.cpp)
target_link_libraries(test_vrtsim PRIVATE vrtsim_static UTIL CONFIG_LIB GTest::gtest dl shlib_loader)
if (OAI_VRTSIM_TAPS_CLIENT)
  target_link_libraries(test_vrtsim PRIVATE taps_client nanomsg)
endif()
add_dependencies(tests test_vrtsim)
add_test(NAME test_vrtsim COMMAND ./test_vrtsim)

if (OAI_VRTSIM_TAPS_CLIENT)
  add_executable(test_vrtsim_taps test_vrtsim_taps.cpp)
  target_link_libraries(test_vrtsim_taps PRIVATE vrtsim_static UTIL CONFIG_LIB GTest::gtest dl shlib_loader taps_client nanomsg flatbuffers taps_api)
  add_dependencies(tests test_vrtsim_taps)
  add_test(NAME test_vrtsim_taps COMMAND ./test_vrtsim_taps)
endif()

add_executable(test_vrtsim_cirdb test_vrtsim_cirdb.cpp)
target_link_libraries(test_vrtsim_cirdb PRIVATE vrtsim_static UTIL CONFIG_LIB GTest::gtest dl shlib_loader yaml-cpp)
add_dependencies(tests test_vrtsim_cirdb)
add_test(NAME test_vrtsim_cirdb COMMAND ./test_vrtsim_cirdb)
