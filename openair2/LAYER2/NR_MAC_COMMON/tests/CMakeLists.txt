add_executable(test_lut_dmrs test_lut_dmrs.c)
target_link_libraries(test_lut_dmrs PRIVATE
    MAC_NR_COMMON
    nr_common
    asn1_nr_rrc_hdrs
    minimal_lib m)
add_dependencies(tests test_lut_dmrs)
add_test(NAME test_lut_dmrs COMMAND test_lut_dmrs)

add_executable(test_sib19_epoch test_sib19_epoch.c)
target_link_libraries(test_sib19_epoch PRIVATE
    MAC_NR_COMMON
    nr_common
    asn1_nr_rrc_hdrs
    minimal_lib m)
add_dependencies(tests test_sib19_epoch)
add_test(NAME test_sib19_epoch COMMAND test_sib19_epoch)
