# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(UE_ID_COMMON_E2SM_SRCS
  ../flexric/src/sm/rc_sm/ie/asn/AMFPointer.c
  ../flexric/src/sm/rc_sm/ie/asn/AMFRegionID.c
  ../flexric/src/sm/rc_sm/ie/asn/AMFSetID.c
  ../flexric/src/sm/rc_sm/ie/asn/AMF-UE-NGAP-ID.c
  ../flexric/src/sm/rc_sm/ie/asn/ENB-ID.c
  ../flexric/src/sm/rc_sm/ie/asn/ENB-UE-X2AP-ID-Extension.c
  ../flexric/src/sm/rc_sm/ie/asn/ENB-UE-X2AP-ID.c
  ../flexric/src/sm/rc_sm/ie/asn/GlobalENB-ID.c
  ../flexric/src/sm/rc_sm/ie/asn/GlobalGNB-ID.c
  ../flexric/src/sm/rc_sm/ie/asn/GlobalNgENB-ID.c
  ../flexric/src/sm/rc_sm/ie/asn/GlobalNGRANNodeID.c
  ../flexric/src/sm/rc_sm/ie/asn/GNB-CU-CP-UE-E1AP-ID.c
  ../flexric/src/sm/rc_sm/ie/asn/GNB-CU-UE-F1AP-ID.c
  ../flexric/src/sm/rc_sm/ie/asn/GNB-ID.c
  ../flexric/src/sm/rc_sm/ie/asn/GUAMI.c
  ../flexric/src/sm/rc_sm/ie/asn/GUMMEI.c
  ../flexric/src/sm/rc_sm/ie/asn/MME-Code.c
  ../flexric/src/sm/rc_sm/ie/asn/MME-Group-ID.c
  ../flexric/src/sm/rc_sm/ie/asn/MME-UE-S1AP-ID.c
  ../flexric/src/sm/rc_sm/ie/asn/NGENB-CU-UE-W1AP-ID.c
  ../flexric/src/sm/rc_sm/ie/asn/NgENB-ID.c
  ../flexric/src/sm/rc_sm/ie/asn/NG-RANnodeUEXnAPID.c
  ../flexric/src/sm/rc_sm/ie/asn/PLMNIdentity.c
  ../flexric/src/sm/rc_sm/ie/asn/RANUEID.c
  ../flexric/src/sm/rc_sm/ie/asn/UEID-ENB.c
  ../flexric/src/sm/rc_sm/ie/asn/UEID-EN-GNB.c
  ../flexric/src/sm/rc_sm/ie/asn/UEID-GNB-CU-CP-E1AP-ID-Item.c
  ../flexric/src/sm/rc_sm/ie/asn/UEID-GNB-CU-CP-E1AP-ID-List.c
  ../flexric/src/sm/rc_sm/ie/asn/UEID-GNB-CU-CP-F1AP-ID-Item.c
  ../flexric/src/sm/rc_sm/ie/asn/UEID-GNB-CU-F1AP-ID-List.c
  ../flexric/src/sm/rc_sm/ie/asn/UEID-GNB-CU-UP.c
  ../flexric/src/sm/rc_sm/ie/asn/UEID-GNB-DU.c
  ../flexric/src/sm/rc_sm/ie/asn/UEID-GNB.c
  ../flexric/src/sm/rc_sm/ie/asn/UEID.c
  ../flexric/src/sm/rc_sm/ie/asn/UEID-NG-ENB-DU.c
  ../flexric/src/sm/rc_sm/ie/asn/UEID-NG-ENB.c
)

add_library(e2_ran_func_cuup STATIC
            ${UE_ID_COMMON_E2SM_SRCS}
            init_ran_func.c
            read_setup_ran.c
            O-RAN/ran_e2sm_ue_id.c
            O-RAN/ran_func_kpm.c
            O-RAN/ran_func_kpm_subs.c
            O-RAN/ran_func_rc.c
            O-RAN/ran_func_rc_subs.c
            ../flexric/src/sm/rc_sm/ie/rc_data_ie.c
            CUSTOMIZED/ran_func_gtp.c    # GTP SM not yet implemented in CU-UP
            CUSTOMIZED/ran_func_pdcp.c
            CUSTOMIZED/ran_func_tc.c     # currently, not implemented; therefore, filling rnd data
            ../flexric/test/rnd/fill_rnd_data_tc.c
            )

target_link_libraries(e2_ran_func_cuup PUBLIC asn1_nr_rrc nr_rrc asn1_nr_rrc_hdrs e2_time_obj kpm_ric_info_common_obj kpm_ric_info_obj 3gpp_derived_ie_obj e2sm_rc_ir_obj sm_common_ie_obj ds alg sm_common_enc_asn_obj_rc 3gpp_derived_ie_enc_asn_obj_rc)
target_compile_definitions(e2_ran_func_cuup PUBLIC ${E2AP_VERSION}  ${KPM_VERSION}  NGRAN_GNB_CUUP)


add_library(e2_ran_func_du_cucp_cuup STATIC
            ${UE_ID_COMMON_E2SM_SRCS}
            init_ran_func.c
            read_setup_ran.c
            O-RAN/ran_e2sm_ue_id.c
            O-RAN/ran_func_kpm.c
            O-RAN/ran_func_kpm_subs.c
            O-RAN/ran_func_rc.c
            O-RAN/ran_func_rc_subs.c
            ../flexric/src/sm/rc_sm/ie/rc_data_ie.c
            CUSTOMIZED/ran_func_gtp.c
            CUSTOMIZED/ran_func_pdcp.c
            CUSTOMIZED/ran_func_mac.c
            CUSTOMIZED/ran_func_rlc.c
            CUSTOMIZED/ran_func_slice.c  # currently, not implemented; therefore, filling rnd data
            ../flexric/test/rnd/fill_rnd_data_slice.c
            CUSTOMIZED/ran_func_tc.c     # currently, not implemented; therefore, filling rnd data
            ../flexric/test/rnd/fill_rnd_data_tc.c
            )

target_link_libraries(e2_ran_func_du_cucp_cuup PUBLIC asn1_nr_rrc nr_rrc asn1_nr_rrc_hdrs e2_time_obj kpm_ric_info_common_obj kpm_ric_info_obj 3gpp_derived_ie_obj e2sm_rc_ir_obj sm_common_ie_obj ds sm_common_enc_asn_obj_rc 3gpp_derived_ie_enc_asn_obj_rc)
target_include_directories(e2_ran_func_du_cucp_cuup PRIVATE
  ${CMAKE_SOURCE_DIR}/openair2/LAYER2/NR_MAC_gNB
  ${CMAKE_SOURCE_DIR}/openair2/LAYER2/NR_MAC_gNB/slice_prb_allocator)
target_compile_definitions(e2_ran_func_du_cucp_cuup PUBLIC ${E2AP_VERSION}  ${KPM_VERSION} NGRAN_GNB_DU NGRAN_GNB_CUCP NGRAN_GNB_CUUP)

# Current implementation:
# - we only have one executable nr-softmodem for 4 node types: DU, CU, gNB-mono, CU-CP

# In the future, when nr-softmodem is separated, linking should be done as following:
# - DU: e2_ran_func_du
# - CU-CP : e2_ran_func_cucp
# - CU-UP: e2_ran_func_cuup (this is available at the moment, executable nr-cuup)
# - CU: e2_ran_func_cucp + e2_ran_func_cuup
# - gNB-mono: e2_ran_func_du + e2_ran_func_cucp + e2_ran_func_cuup
