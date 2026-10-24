add_executable(test_lut_dmrs test_lut_dmrs.c)
target_link_libraries(test_lut_dmrs PRIVATE
    MAC_NR_COMMON
    nr_common
    asn1_nr_rrc_hdrs
    minimal_lib m)
add_dependencies(tests test_lut_dmrs)
add_test(NAME test_lut_dmrs COMMAND test_lut_dmrs)

add_executable(test_csi_bitlen test_csi_bitlen.c)
target_link_libraries(test_csi_bitlen PRIVATE
    MAC_NR_COMMON
    nr_common
    asn1_nr_rrc_hdrs
    minimal_lib m)
add_dependencies(tests test_csi_bitlen)
add_test(NAME test_csi_bitlen COMMAND test_csi_bitlen)
