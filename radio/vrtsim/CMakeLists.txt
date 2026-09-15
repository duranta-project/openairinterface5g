# SPDX-License-Identifier: LicenseRef-CSSL-1.0

# vrtsim module
add_library(vrtsim MODULE
  vrtsim.c
  cirdb_provider.c
  cirdb_yaml.cpp
)

set_target_properties(vrtsim PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_package(yaml-cpp REQUIRED)

# Link libraries - always link yaml-cpp
target_link_libraries(vrtsim
  PRIVATE
    SIMU
    shm_td_iq_channel
    noise_device
    log_headers
    yaml-cpp
    channel_pipeline
    thread-pool
)
if (ENABLE_CHANNEL_SIM_CUDA)
  target_compile_definitions(vrtsim PRIVATE CHANNEL_SIM_CUDA)
endif()

# Optional taps client
add_boolean_option(OAI_VRTSIM_TAPS_CLIENT OFF "Enable taps client" ON)
if (OAI_VRTSIM_TAPS_CLIENT)
  target_link_libraries(vrtsim PRIVATE taps_client)

  download_file(
    "https://gitlab.eurecom.fr/oai/raytracing-channel-emulator/-/raw/59589ad2a534881810b2aa7dd81e63066245bb8d/server/api/taps.fbs?inline=false"
    ${CMAKE_SOURCE_DIR}/taps.fbs
    RETRIES 3
  )

  set(TAPS_IF_PATH ${CMAKE_SOURCE_DIR}/taps.fbs)
  set(TAPS_API_HEADER taps_generated.h)

  add_custom_command(OUTPUT ${TAPS_API_HEADER}
    COMMAND flatc -c ${TAPS_IF_PATH}
    DEPENDS ${TAPS_IF_PATH}
    COMMENT "Generating flatbuffers API from ${TAPS_IF_PATH}"
  )

  add_library(taps_api INTERFACE)
  target_include_directories(taps_api INTERFACE ${CMAKE_CURRENT_BINARY_DIR})

  add_library(taps_client taps_client.cpp ${TAPS_API_HEADER})
  target_include_directories(taps_client PUBLIC .)
  find_package(FlatBuffers QUIET)
  if(NOT FlatBuffers_FOUND)
    # Fall back to lowercase (Ubuntu 22.04)
    find_package(Flatbuffers REQUIRED)
    set(FLATBUFFERS_TARGET flatbuffers)
  else()
    set(FLATBUFFERS_TARGET flatbuffers::flatbuffers)
  endif()
  target_link_libraries(taps_client PUBLIC ${FLATBUFFERS_TARGET})
  target_link_libraries(taps_client PUBLIC flatbuffers taps_api SIMU nanomsg)
endif()


if (ENABLE_TESTS)
  add_library(vrtsim_static vrtsim.c cirdb_provider.c cirdb_yaml.cpp)
  target_link_libraries(vrtsim_static
    PUBLIC
      SIMU
      shm_td_iq_channel
      noise_device
      log_headers
      yaml-cpp
      channel_pipeline
      thread-pool
  )
  if (OAI_VRTSIM_TAPS_CLIENT)
    target_link_libraries(vrtsim_static PRIVATE taps_client)
  endif()
  if (CUDA_ENABLE)
    target_compile_definitions(vrtsim_static PRIVATE CUDA_ENABLE)
  endif()
  add_subdirectory(tests)
endif()
