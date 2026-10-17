# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_subdirectory(nr-cuup)
add_subdirectory(nr-cu-nrppa)
add_subdirectory(nr-ue-nas-simulator)
add_subdirectory(nrdlbench)
if(ENABLE_TESTS AND ENABLE_NTN_ASSISTANCE)
  add_subdirectory(ntn-assistance)
endif()
