# SPDX-License-Identifier: LicenseRef-CSSL-1.0

# Opt-in build-time gate: a build cannot know which E3Configuration.encoding a
# conf will select, so this is empty by default and the checks below only warn.
# A deployment that does know its conf can pin it (-DE3_REQUIRED_ENCODINGS=json).
set(E3_REQUIRED_ENCODINGS "" CACHE STRING "E3 wire encodings the linked libe3 must provide (asn1;json;protobuf); empty = no build-time requirement")

find_package(PkgConfig REQUIRED)
# libe3's C API is not stable across minor releases: 0.2.0 adds a correlation
# id (sequence_id) to the control and report callbacks and to
# e3_agent_send_xapp_control(), so stay inside the 0.2 series. It also covers
# the .pc Cflags below: 0.1.3 is the first release whose Cflags are safe to
# consume as compile definitions (earlier ones exported a quoted
# -DLATREC_DEFAULT_DIR whose quotes pkg-config strips).
pkg_check_modules(CLIBE3 REQUIRED libe3>=0.2.0 libe3<0.3.0)

if(NOT CLIBE3_FOUND)
    message(FATAL_ERROR "libe3 not found and required for E3 Agent")
endif()

# Two constraints make pkg_check_modules report the version per module.
message(STATUS "Using for E3AP libe3 version: ${CLIBE3_libe3_VERSION}")

# libe3's feature macros, as bare names for target_compile_definitions().
# libe3.pc is generated from the libe3 target's INTERFACE_COMPILE_DEFINITIONS,
# so what this reports cannot drift from the binary we link.
set(CLIBE3_COMPILE_DEFS "")
foreach(_def IN LISTS CLIBE3_CFLAGS_OTHER)
    if(_def MATCHES "^-D(.+)$")
        list(APPEND CLIBE3_COMPILE_DEFS "${CMAKE_MATCH_1}")
    else()
        message(STATUS "Ignoring non-define flag in libe3.pc Cflags: ${_def}")
    endif()
endforeach()
unset(_def)
message(STATUS "libe3 capabilities: ${CLIBE3_COMPILE_DEFS}")

# The E3SM codecs compile every wire format and pick at runtime, so which
# encoders libe3 has is the only thing deciding what this gNB can serve. An
# encoding libe3 lacks previously surfaced only as a null encoder inside libe3,
# reported as a generic agent-init failure that named nothing.
set(CLIBE3_ENCODINGS "")
foreach(_enc asn1 json protobuf)
    string(TOUPPER "${_enc}" _enc_upper)
    if("LIBE3_ENABLE_${_enc_upper}" IN_LIST CLIBE3_COMPILE_DEFS)
        list(APPEND CLIBE3_ENCODINGS "${_enc}")
    endif()
endforeach()
message(STATUS "libe3 E3AP encodings: ${CLIBE3_ENCODINGS}")

# No encoder at all is a broken libe3, not a deployment choice.
if(NOT CLIBE3_ENCODINGS)
    message(FATAL_ERROR
        "libe3 ${CLIBE3_libe3_VERSION} was built with no E3AP encoder at all "
        "(none of LIBE3_ENABLE_{ASN1,JSON,PROTOBUF}); every value of "
        "E3Configuration.encoding would fail at gNB startup. Rebuild libe3 "
        "with at least one of them ON.")
endif()

# Not an error: a JSON-only deployment builds libe3 with ASN1=OFF on purpose
# and sets the key explicitly. e3_readconfig() fails the run if one does not.
if(NOT "asn1" IN_LIST CLIBE3_ENCODINGS)
    message(WARNING
        "libe3 ${CLIBE3_libe3_VERSION} has no asn1 encoder, but asn1 is the "
        "E3Configuration.encoding default: every gNB conf used with this build "
        "must set encoding explicitly (to one of ${CLIBE3_ENCODINGS}) or the "
        "run aborts at startup.")
endif()

foreach(_enc IN LISTS E3_REQUIRED_ENCODINGS)
    # Checked against the accepted names first, so a typo reads as a typo
    # rather than as "libe3 was built without <nonexistent encoder>".
    if(NOT _enc MATCHES "^(asn1|json|protobuf)$")
        message(FATAL_ERROR
            "E3_REQUIRED_ENCODINGS: unknown encoding '${_enc}' "
            "(expected asn1, json or protobuf)")
    endif()
    if(NOT _enc IN_LIST CLIBE3_ENCODINGS)
        string(TOUPPER "${_enc}" _enc_upper)
        message(FATAL_ERROR
            "E3_REQUIRED_ENCODINGS lists '${_enc}' but libe3 "
            "${CLIBE3_libe3_VERSION} was built without it (it has: "
            "${CLIBE3_ENCODINGS}). Rebuild libe3 with "
            "-DLIBE3_ENABLE_${_enc_upper}=ON, or drop '${_enc}' from "
            "E3_REQUIRED_ENCODINGS.")
    endif()
endforeach()
unset(_enc)
unset(_enc_upper)

include_directories(${CLIBE3_INCLUDE_DIRS})

add_subdirectory(service_models)
add_library(e3ap e3_agent.c config/e3_config.c)

# target_link_libraries(e3ap PUBLIC OCP_ITTI) # TODO

# pkg-config tells us where libe3 lives; without this the -llibe3 above only
# resolves when libe3 happens to sit in a default linker path.
target_link_directories(e3ap PUBLIC ${CLIBE3_LIBRARY_DIRS})
target_link_libraries(e3ap PUBLIC ${CLIBE3_LIBRARIES} ${SERVICE_MODELS_LIBRARY})

target_compile_definitions(e3ap PRIVATE ${CLIBE3_COMPILE_DEFS})

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
