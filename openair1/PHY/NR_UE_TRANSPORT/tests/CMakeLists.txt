# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_nr_ulsch_ue_uci_on_pusch test_nr_ulsch_ue_uci_on_pusch.c)
target_link_libraries(test_nr_ulsch_ue_uci_on_pusch PRIVATE
  -Wl,--start-group UTIL SIMU PHY_NR_COMMON PHY_NR PHY_NR_UE SCHED_NR_LIB CONFIG_LIB MAC_NR_COMMON -Wl,--end-group
  m pthread ${T_LIB} ITTI dl physim_common
  # The LDPC coding interface is loaded as a shared library (libldpc.so) at
  # runtime via dlopen(). It references 'init_task_ans' (thread-pool) and
  # 'check_crc' (crc_byte) which are not otherwise referenced by this test, so
  # they would not be linked into the executable. --whole-archive forces them
  # in (and -rdynamic exports them) so the shared library can resolve them.
  # See the 'ldpctest' target for the same pattern.
  -Wl,--whole-archive crc_byte thread-pool -Wl,--no-whole-archive
)
target_link_libraries(test_nr_ulsch_ue_uci_on_pusch PRIVATE asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs)
add_dependencies(tests test_nr_ulsch_ue_uci_on_pusch)

# The MATLAB generated vector is downloaded by the fixture below.
add_test(NAME test_nr_ulsch_ue_uci_on_pusch.fixture.download
  COMMAND ${CMAKE_COMMAND}
    "-DURL=https://huggingface.co/datasets/sakipri/uci_on_pusch_test.bin/resolve/main/uci_on_pusch_oack3_ocsi14_ocsi24.bin"
    "-DDEST=${CMAKE_BINARY_DIR}/testdata/input_vector.bin"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/DownloadFile.cmake
)
set_tests_properties(test_nr_ulsch_ue_uci_on_pusch.fixture.download PROPERTIES
  FIXTURES_SETUP nr_ulsch_ue_uci_on_pusch_vector_fixture
)

add_test(NAME test_nr_ulsch_ue_uci_on_pusch
  COMMAND ./test_nr_ulsch_ue_uci_on_pusch ${CMAKE_BINARY_DIR}/testdata/input_vector.bin)
set_tests_properties(test_nr_ulsch_ue_uci_on_pusch PROPERTIES
  FIXTURES_REQUIRED nr_ulsch_ue_uci_on_pusch_vector_fixture
)
