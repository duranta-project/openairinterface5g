# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_nr_csi_rs test_nr_csi_rs.c)
target_include_directories(test_nr_csi_rs PRIVATE
  ${OPENAIR1_DIR}/PHY/nr_phy_common/inc
  ${OPENAIR1_DIR}/PHY/NR_UE_TRANSPORT
)
target_link_libraries(test_nr_csi_rs PRIVATE LOG minimal_lib -lm)
add_dependencies(tests test_nr_csi_rs)
add_test(NAME test_nr_csi_rs COMMAND ./test_nr_csi_rs)
