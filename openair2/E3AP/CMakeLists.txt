# SPDX-License-Identifier: LicenseRef-CSSL-1.0

find_package(PkgConfig REQUIRED)
# libe3's C API is not stable across minor releases: 0.2.0 adds a correlation
# id (sequence_id) to the control and report callbacks and to
# e3_agent_send_xapp_control(), so stay inside the 0.2 series.
pkg_check_modules(CLIBE3 REQUIRED libe3>=0.2.0 libe3<0.3.0)

if(NOT CLIBE3_FOUND)
    message(FATAL_ERROR "libe3 not found and required for E3 Agent")
endif()

# Two constraints make pkg_check_modules report the version per module.
message(STATUS "Using for E3AP libe3 version: ${CLIBE3_libe3_VERSION}")

include_directories(${CLIBE3_INCLUDE_DIRS})

add_subdirectory(service_models)
add_library(e3ap e3_agent.c config/e3_config.c)

# target_link_libraries(e3ap PUBLIC OCP_ITTI) # TODO

# pkg-config tells us where libe3 lives; without this the -llibe3 above only
# resolves when libe3 happens to sit in a default linker path.
target_link_directories(e3ap PUBLIC ${CLIBE3_LIBRARY_DIRS})
target_link_libraries(e3ap PUBLIC ${CLIBE3_LIBRARIES} ${SERVICE_MODELS_LIBRARY})

# DU/MAC-coupled half of the Spectrum RAN function: the scan, slot reservation
# and (Aerial) capture PUSCH that read and write gNB MAC state. Like the E2 RAN
# functions, the node-type specific half is kept apart from the service model
# itself (service_models/spectrum_sm), which stays free of MAC symbols.
#
# It is an OBJECT library linked by L2_NR, not by spectrum_sm/e3ap: e3ap is on
# the link line of every SCHED_NR_LIB consumer (the PHY tap needs it), including
# phy simulators that do not link L2_NR and so could not resolve the MAC symbols
# this code uses. Linking it into L2_NR archives the objects into libL2_NR.a, so
# it reaches exactly the binaries that have a MAC. The weak fallbacks for the
# others are in service_models/spectrum_sm/ran_func_spectrum_stub.c.
add_library(e3_ran_func_du OBJECT
  ran_func_spectrum.c
  ran_func_spectrum_prb_block.c
  ran_func_spectrum_collision.c
  ran_func_spectrum_registry.c)
if(OAI_AERIAL)
  # The capture PUSCH exists only for the Aerial L1. L2_NR carries the same
  # definition for its own sources, but it is not inherited by this target.
  target_sources(e3_ran_func_du PRIVATE ran_func_spectrum_aerial.c)
  target_compile_definitions(e3_ran_func_du PRIVATE ENABLE_AERIAL)
endif()
# nr_mac_gNB.h includes the generated NR RRC ASN.1 headers; log_headers orders the
# object after the generated T_IDs.h that the LOG macros include.
target_link_libraries(e3_ran_func_du PRIVATE asn1_nr_rrc_hdrs log_headers)
target_link_libraries(L2_NR PRIVATE e3_ran_func_du)
