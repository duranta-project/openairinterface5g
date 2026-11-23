# SPDX-License-Identifier: LicenseRef-CSSL-1.0

##########################################################
# LDPC decoding on the Arm RAN Acceleration Library (ArmRAL)
##########################################################

# ArmRAL has to be built with -DCMAKE_POSITION_INDEPENDENT_CODE=ON to link into this module, see README.md
find_package(armral REQUIRED)

# same as libldpc.so (OAI segment coding and encoder), with the code block decoder on ArmRAL
add_library(ldpc_armral MODULE
            nrLDPC_armral_decoder.c
            ${OPENAIR1_DIR}/PHY/CODING/nrLDPC_encoder/ldpc_encoder_optim8segmulti.c
)
set_target_properties(ldpc_armral PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(ldpc_armral PRIVATE ldpc_segment armral)
# keep ArmRAL's symbols local: modules are loaded RTLD_GLOBAL, and the FHI 7.2 library links its own copy
target_link_options(ldpc_armral PRIVATE "LINKER:--exclude-libs,ALL")

add_dependencies(ldpctest ldpc_armral)
add_dependencies(nr-softmodem ldpc_armral)
add_dependencies(nr-uesoftmodem ldpc_armral)
add_dependencies(nr_ulsim ldpc_armral)
add_dependencies(nr_ulschsim ldpc_armral)
add_dependencies(nr_dlsim ldpc_armral)
add_dependencies(nr_dlschsim ldpc_armral)

if (ENABLE_PHYSIM_TESTS)
  # cases of physim.5g.ldpctest (one code block per trial); not block length 1, see README.md
  add_physim_test(ldpctest.ldpc_armral.bg1_3872 "ldpctest: ArmRAL decoder, block length 3872, BG1" ldpctest -v _armral -l3872 -s10 -n300)
  add_physim_test(ldpctest.ldpc_armral.bg1_8448 "ldpctest: ArmRAL decoder, block length 8448, BG1" ldpctest -v _armral -l8448 -s10 -n300)
  add_physim_test(ldpctest.ldpc_armral.bg2_r15 "ldpctest: ArmRAL decoder, block length 3000, BG2, rate 1/5" ldpctest -v _armral -l3000 -r1 -d5 -s10 -n300)
  add_physim_test(nr_dlsim.ldpc_armral.106prb "nr_dlsim: ArmRAL decoder, 106 PRBs, MCS 20" nr_dlsim -n100 -e20 -s30 -R106 -b106 -P --loader.ldpc.shlibversion _armral)
  add_physim_test(nr_dlsim.ldpc_armral.bg2 "nr_dlsim: ArmRAL decoder, 24 PRBs, MCS 5 (base graph 2)" nr_dlsim -n100 -e5 -s10 -R24 -b24 -P --loader.ldpc.shlibversion _armral)
  add_physim_test(nr_ulsim.ldpc_armral.106prb "nr_ulsim: ArmRAL decoder, 106 PRBs, MCS 16" nr_ulsim -n100 -m16 -s15 -R106 -r106 -P --loader.ldpc.shlibversion _armral)
  # high code rates (LLR scaling, see README.md)
  add_physim_test(nr_dlsim.ldpc_armral.256qam "nr_dlsim: ArmRAL decoder, 51 PRBs, MCS 26 of table 2 (256QAM)" nr_dlsim -n100 -e26 -q1 -s28 -S28.1 -R51 -b51 -P --loader.ldpc.shlibversion _armral)
  add_physim_test(nr_ulsim.ldpc_armral.mcs27 "nr_ulsim: ArmRAL decoder, 51 PRBs, MCS 27 (64QAM)" nr_ulsim -n100 -m27 -s23 -S23.1 -R51 -r51 -P --loader.ldpc.shlibversion _armral)
endif()
