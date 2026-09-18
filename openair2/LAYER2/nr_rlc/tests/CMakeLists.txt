# SPDX-License-Identifier: LicenseRef-CSSL-1.0

foreach(TESTNUM RANGE 1 17)
  add_executable(nr_rlc_test_${TESTNUM} test.c)
  target_link_libraries(nr_rlc_test_${TESTNUM} PRIVATE nr_rlc_core pthread utils minimal_lib)
  target_compile_definitions(nr_rlc_test_${TESTNUM} PRIVATE TEST="test${TESTNUM}.h")
  #target_include_directories(nr_rlc_test_${TESTNUM} PRIVATE LOG)
  #target_compile_options(nr_rlc_test_${TESTNUM} PRIVATE -g --coverage -Wall)
  add_dependencies(tests nr_rlc_test_${TESTNUM})
endforeach()

add_test(NAME nr_rlc_tests
  COMMAND exec_nr_rlc_test.sh ${CMAKE_CURRENT_BINARY_DIR}
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_nr_rlc_am_entity test_nr_rlc_am_entity.cpp)
target_link_libraries(test_nr_rlc_am_entity PRIVATE nr_rlc_core GTest::gtest minimal_lib)
add_dependencies(tests test_nr_rlc_am_entity)
add_test(NAME test_nr_rlc_am_entity COMMAND ./test_nr_rlc_am_entity)

add_executable(benchmark_nr_rlc_am_entity benchmark_nr_rlc_am_entity.cpp)
target_link_libraries(benchmark_nr_rlc_am_entity PRIVATE benchmark::benchmark nr_rlc_core UTIL ${T_LIB} minimal_lib)
add_dependencies(tests benchmark_nr_rlc_am_entity)
add_test(NAME benchmark_nr_rlc_am_entity
  COMMAND ./benchmark_nr_rlc_am_entity)

add_executable(test_retired_nacks test_retired_nacks.c)
target_link_libraries(test_retired_nacks PRIVATE nr_rlc_core minimal_lib)
add_dependencies(tests test_retired_nacks)
add_test(NAME test_retired_nacks COMMAND test_retired_nacks)
