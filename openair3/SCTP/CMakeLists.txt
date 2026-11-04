# SPDX-License-Identifier: LicenseRef-CSSL-1.0

find_package(sctp REQUIRED)
# Optional userspace SCTP stack for F1-C and N2, used when the kernel has no
# CONFIG_IP_SCTP. This switch only decides whether the backend is COMPILED IN --
# which backend is USED is decided at runtime in oai_sctp.c, by trying to open a
# kernel SCTP socket. It has to be that way round: OAI is cross-compiled, so a
# configure-time probe would test the build host's kernel rather than the
# target's, and one image may be booted against kernels that differ in SCTP
# support. See the header comment in openair3/SCTP/oai_sctp.h.
option(ENABLE_USRSCTP "Build the usrsctp fallback backend for F1-C/N2" OFF)
add_library(SCTP_CLIENT STATIC
  sctp_common.c
  sctp_eNB_task.c
  sctp_eNB_itti_messaging.c
  oai_sctp.c
)
target_include_directories(SCTP_CLIENT PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SCTP_CLIENT PRIVATE log_headers)
target_link_libraries(SCTP_CLIENT PRIVATE sctp::sctp)
target_link_libraries(SCTP_CLIENT PRIVATE asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs)

if(ENABLE_USRSCTP)
  # Use the system-installed usrsctp (e.g., usrsctp-devel/libusrsctp-dev); fail if absent.
  pkg_check_modules(usrsctp REQUIRED IMPORTED_TARGET usrsctp)
  # The backend lives in its own target: <usrsctp.h> and <netinet/sctp.h>
  # declare the same names and cannot share a translation unit, so the usrsctp
  # headers are confined to the one source file that wants them.
  add_library(oai_sctp_usrsctp STATIC oai_sctp_usrsctp.c)
  target_compile_definitions(oai_sctp_usrsctp PRIVATE ENABLE_USRSCTP)
  target_link_libraries(oai_sctp_usrsctp PRIVATE PkgConfig::usrsctp)
  target_compile_definitions(SCTP_CLIENT PRIVATE ENABLE_USRSCTP)
  target_link_libraries(SCTP_CLIENT PRIVATE oai_sctp_usrsctp)
endif()
