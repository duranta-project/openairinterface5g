# SPDX-License-Identifier: LicenseRef-CSSL-1.0

##########################################################
# LDPC offload library - O-RAN AAL/DPDK BBDEV
##########################################################

add_boolean_option(ENABLE_LDPC_AAL OFF "Build support for LDPC Offload using O-RAN AAL library" OFF)

if (ENABLE_LDPC_AAL)
  pkg_check_modules(LIBDPDK_AAL REQUIRED libdpdk>=20.11.9)

  find_library(PMD_T2 NAMES rte_baseband_accl_ldpc HINTS ${LIBDPDK_AAL_LIBRARY_DIRS})
  if (PMD_T2)
    message(STATUS "found poll-mode driver for AccelerComm T2 LDPC Offload (rte_baseband_accl_ldpc.so)")
  else()
    if (LIBDPDK_AAL_VERSION VERSION_LESS "22.11")
      message(FATAL_ERROR "for non-T2 BBDEV PMDs, we support the Intel ACC100 and ACC200 (VRB1), using DPDK 22.11 or later.")
    endif()
  endif()

  add_library(ldpc_aal MODULE nrLDPC_coding_aal.c bbdev_oai_sw.c)

  set_target_properties(ldpc_aal PROPERTIES COMPILE_FLAGS "-DALLOW_EXPERIMENTAL_API -DALLOW_INTERNAL_API")
  set_target_properties(ldpc_aal PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  target_include_directories(ldpc_aal PRIVATE ${LIBDPDK_AAL_INCLUDE_DIRS})
  target_link_libraries(ldpc_aal PRIVATE log_headers ${LIBDPDK_AAL_LDFLAGS})
  # baseband_oai_sw registers a vdev (rte_vdev_register is in librte_bus_vdev) and dlopen()s libldpc.so
  target_link_libraries(ldpc_aal PRIVATE -lrte_bus_vdev)
  target_link_libraries(ldpc_aal PRIVATE ${CMAKE_DL_LIBS})
  add_dependencies(ldpc_aal ldpc)
  if (PMD_T2)
    target_link_libraries(ldpc_aal PRIVATE ${PMD_T2})
  endif()

  add_dependencies(nr-softmodem ldpc_aal)
  add_dependencies(nr-uesoftmodem ldpc_aal)
  add_dependencies(nr_ulsim ldpc_aal)
  add_dependencies(nr_ulschsim ldpc_aal)
  add_dependencies(nr_dlsim ldpc_aal)
  add_dependencies(nr_dlschsim ldpc_aal)

endif()
