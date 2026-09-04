# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_rfsimulator test_rfsimulator.cpp)
target_link_libraries(test_rfsimulator PRIVATE rfsimulator_static UTIL CONFIG_LIB GTest::gtest dl shlib_loader)
add_dependencies(tests test_rfsimulator)
add_test(NAME test_rfsimulator COMMAND ./test_rfsimulator)
# Guard against a real regression reintroducing the connect-time hang this test covers:
# without a bound, a stuck client-connect handshake would spin forever instead of failing.
set_tests_properties(test_rfsimulator PROPERTIES TIMEOUT 10)
