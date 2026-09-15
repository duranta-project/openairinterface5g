# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(fgs_5gsm_lib OBJECT PduSessionEstablishRequest.c
                            PduSessionEstablishmentAccept.c
                            PduSessionModificationCommand.c
                            PduSessionModificationComplete.c)

target_link_libraries(fgs_5gsm_lib PUBLIC nas_common_ies_lib log_headers packet_filter_lib)

target_include_directories(fgs_5gsm_lib PUBLIC . ../../)
