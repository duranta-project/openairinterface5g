# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_compile_options(-Wunused-parameter)
find_package(UHD REQUIRED UHD)
message(STATUS "UHD version: ${UHD_VERSION}")

add_library(oai_usrpdevif MODULE usrp_lib.cpp usrp_converters.cpp)
target_link_libraries(oai_usrpdevif PRIVATE ${UHD_LIBRARIES})
target_include_directories(oai_usrpdevif PRIVATE ${UHD_INCLUDE_DIRS})
target_link_libraries(oai_usrpdevif PRIVATE log_headers)
if (UHD_VERSION VERSION_EQUAL "4.11.0")
  # linking Boost::thread seems to be required for UHD v4.11.0.0
  find_package(Boost REQUIRED)
  target_link_libraries(oai_usrpdevif PRIVATE Boost::thread)
endif ()
set_target_properties(oai_usrpdevif PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_custom_command(TARGET oai_usrpdevif POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink liboai_usrpdevif.so liboai_device.so
                   WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
