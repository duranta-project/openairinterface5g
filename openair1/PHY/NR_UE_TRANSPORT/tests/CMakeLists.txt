# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_nr_csi_im test_nr_csi_im.c ../csi_rx.c)
target_link_libraries(test_nr_csi_im PRIVATE asn1_nr_rrc_hdrs nr_phy_common minimal_lib m)
# Keep the real CSI-IM procedure without pulling in unrelated CSI-RS processing.
# Override the global -rdynamic so unused public functions can also be discarded.
target_compile_options(test_nr_csi_im PRIVATE -ffunction-sections -fdata-sections)
target_link_options(test_nr_csi_im PRIVATE -Wl,--gc-sections -Wl,--no-export-dynamic)
add_dependencies(tests test_nr_csi_im)
add_test(NAME test_nr_csi_im COMMAND test_nr_csi_im)
