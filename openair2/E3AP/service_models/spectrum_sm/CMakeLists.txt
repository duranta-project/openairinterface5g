# SPDX-License-Identifier: LicenseRef-CSSL-1.0

# Spectrum Service Model CMakeLists.txt

cmake_minimum_required(VERSION 3.10)

add_subdirectory(MESSAGES)

# Spectrum SM source files
set(SPECTRUM_SM_SOURCES
    spectrum_sm.c
    spectrum_enc.c
    spectrum_sensing_ring.c
    ${CMAKE_SOURCE_DIR}/openair2/LAYER2/NR_MAC_gNB/nr_mac_sensing_stub.c
)

# Create spectrum SM library
add_library(spectrum_sm STATIC ${SPECTRUM_SM_SOURCES})

# Include directories
target_include_directories(spectrum_sm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_BINARY_DIR}/MESSAGES  # ASN.1 generated headers
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common/utils/LOG
    ${CMAKE_SOURCE_DIR}/openair1
    ${CMAKE_SOURCE_DIR}/openair2
    ${CMAKE_SOURCE_DIR}/openair2/E3AP
    ${CLIBE3_INCLUDE_DIRS}  
)

target_compile_definitions(spectrum_sm PRIVATE E3_AGENT)

target_link_directories(spectrum_sm PRIVATE ${CLIBE3_LIBRARY_DIRS})
target_link_libraries(spectrum_sm spectrum_sm_asn1 json-c rt pthread LOG)
add_dependencies(spectrum_sm spectrum_sm_asn1)

if(SPECTRUM_SM_HAVE_PROTOBUF)
  target_link_libraries(spectrum_sm spectrum_sm_pb)
  target_include_directories(spectrum_sm PUBLIC ${PROTOBUFC_INCLUDE_DIRS})
  target_compile_definitions(spectrum_sm PRIVATE E3_SM_HAVE_PROTOBUF)
endif()

# Both encodings are compiled in; the active one is selected at runtime from
# the config file (E3Configuration.encoding).

# Export spectrum SM library
set(SPECTRUM_SM_LIBRARY spectrum_sm PARENT_SCOPE)