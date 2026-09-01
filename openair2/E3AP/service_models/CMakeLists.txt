# SPDX-License-Identifier: LicenseRef-CSSL-1.0

cmake_minimum_required(VERSION 3.10)

# Shared SM plumbing both SMs plug into: the telemetry-worker driver
# (e3_sm_worker.h) and the POSIX-shm region lifecycle (e3_shm_region.h).
add_library(e3_sm_worker STATIC e3_sm_worker.c e3_shm_region.c)
target_include_directories(e3_sm_worker PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common/utils/LOG
    ${CLIBE3_INCLUDE_DIRS}
)
target_link_directories(e3_sm_worker PRIVATE ${CLIBE3_LIBRARY_DIRS})
target_link_libraries(e3_sm_worker pthread LOG)

add_subdirectory(spectrum_sm)
add_subdirectory(l1_kpm_sm)

add_library(service_models INTERFACE)

target_include_directories(service_models INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common/utils/LOG
    ${CMAKE_SOURCE_DIR}/openair2/E3AP
    ${CLIBE3_INCLUDE_DIRS} 
)

target_link_libraries(service_models INTERFACE
    ${SPECTRUM_SM_LIBRARY}
    ${L1_KPM_SM_LIBRARY}
    e3_sm_worker
)

set(SERVICE_MODELS_LIBRARY service_models PARENT_SCOPE)