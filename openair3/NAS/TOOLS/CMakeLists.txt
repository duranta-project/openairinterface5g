# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_subdirectory(sim_http)

add_library(conf2uedata_lib OBJECT
    conf_emm.c
    conf_user_data.c
    conf_usim.c
    conf_network.c
    conf_user_plmn.c
    conf_parser.c
    fs.c
    display.c
    ../UE/API/USIM/usim_api.c
    ../UE/API/USIM/aka_functions.c
    ../COMMON/UTIL/OctetString.c
    ${OPENAIR_DIR}/common/utils/mem/memory.c
    ${OPENAIR_DIR}/common/utils/utils.c
)
target_link_libraries(conf2uedata_lib PUBLIC log_headers)

add_executable(conf2uedata conf2uedata.c)
target_link_libraries(conf2uedata PRIVATE conf2uedata_lib)
set_target_properties(conf2uedata PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(conf2uedata PRIVATE LOG minimal_lib ${libconfig_LIBRARIES})

add_executable(usim usim.c)
target_link_libraries(usim PRIVATE conf2uedata_lib)
set_target_properties(usim PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(usim PRIVATE LOG minimal_lib ${libconfig_LIBRARIES})

add_executable(nvram nvram.c)
target_link_libraries(nvram PRIVATE conf2uedata_lib)
set_target_properties(nvram PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(nvram PRIVATE LOG minimal_lib ${libconfig_LIBRARIES})
