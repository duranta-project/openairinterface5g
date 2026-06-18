enable_language(CUDA)

# it would be possible to auto-detect compute capailities: https://stackoverflow.com/a/68223399
#include(FindCUDA/select_compute_arch)
#CUDA_DETECT_INSTALLED_GPUS(INSTALLED_GPU_CCS_1)

# Nvidia recommends to build against all targeted archs: https://forums.developer.nvidia.com/t/261162
# this is for cmake 3.16 (OAI minimum), starting at cmake 3.18 there is another syntax, see below

SET(CUDA_VERBOSE_BUILD ON)

# version using NVLink-C2C in Grace Hopper
add_library(ldpc_cuda MODULE
            # main entry
            nrLDPC_coding_cuda.c

            nrLDPC_decoder_cuda.c
            ldpc_encoder_cuda32.c
            ldpc_input32.c

            nrLDPC_coding_segment_encoder_cuda.c
            nrLDPC_coding_segment_decoder_cuda.c

            # .cu files
            nrLDPC_decoder_BG1_cuda.cu
            ldpc_BG1_Zc384_32bit.cu
            ldpc_input.cu
            ldpc_deinterleave.cu
            ldpc_rate_matching.cu
)
set_target_properties(ldpc_cuda PROPERTIES CUDA_ARCHITECTURES "80;87;90")
# we need to find the right include directory, but ti seems that
# enable_language(CUDA) does not make this available(?), so also use the CUDA package
find_package(CUDA REQUIRED)
target_include_directories(ldpc_cuda PRIVATE ${CUDA_INCLUDE_DIRS})

target_link_libraries(ldpc_cuda ldpc_gen_HEADERS log_headers)
set_target_properties(ldpc_cuda PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
set_target_properties(ldpc_cuda PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_dependencies(ldpctest ldpc_cuda)
add_dependencies(nr-softmodem ldpc_cuda)
add_dependencies(nr-uesoftmodem ldpc_cuda)
add_dependencies(nr_ulsim ldpc_cuda)
add_dependencies(nr_ulschsim ldpc_cuda)
add_dependencies(nr_dlsim ldpc_cuda)
add_dependencies(nr_dlschsim ldpc_cuda)

if(ENABLE_PHYSIM_TESTS)

  # this is a list of physical simulators designed to test the functioning of
  # ldpc_cuda. Of course, one could also modify
  # openair1/SIMULATION/tests/CMakeLists.txt to run them with ldpc_cuda

  add_physim_test(ldpctest.ldpc_cuda.simple "ldpctest: simple CUDA load check" ldpctest -v _cuda -l8448 -s10 -n300)
  add_physim_test(ldpctest.ldpc_cuda.r13 "ldpctest: base graph 1, rate 1/3" ldpctest -v _cuda -r1 -n300 -S64 -s4 -i4)
  add_physim_test(ldpctest.ldpc_cuda.r23 "ldpctest: base graph 1, rate 2/3" ldpctest -v _cuda -r2 -n300 -S64 -s4 -i4)
  add_physim_test(ldpctest.ldpc_cuda.r89 "ldpctest: base graph 1, rate 8/9" ldpctest -v _cuda -r22 -d25 -n300 -S64 -s4 -i4)

  add_physim_test(nr_dlsim.ldpc_cuda.9seg_r13 "nr_dlsim: 9 segments, rate 1/3" nr_dlsim -n100 -s20 -e13 -R273 -b273 -x1 -y1 -z1 -P --loader.ldpc.shlibversion _cuda)
  add_physim_test(nr_dlsim.ldpc_cuda.18seg_r23 "nr_dlsim: 18 segments, rate 2/3" nr_dlsim -n100 -s20 -e22 -R273 -b273 -x1 -y1 -z1 -P --loader.ldpc.shlibversion _cuda)
  add_physim_test(nr_dlsim.ldpc_cuda.50seg_r89 "nr_dlsim: 50 segments, rate 8/9" nr_dlsim -n100 -s40 -e27 -R273 -b273 -z2 -x2 -y2 -P --loader.ldpc.shlibversion _cuda)
  add_physim_test(nr_dlsim.ldpc_cuda.2t2r_mcs25_64qam "nr_dlsim: 2T2R/2 layers, MCS 25, 64QAM, 100 MHz 30 kHz SCS)" nr_dlsim -n100 -R273 -b273 -P -e 25 -s30 -x2 -y2 -z2 --loader.ldpc.shlibversion _cuda)
  add_physim_test(nr_dlsim.ldpc_cuda.1t1r_mcs25_256qam "nr_dlsim: 1T1R/1 layer, MCS 25, 256QAM, 100 MHz 30 kHz SCS)" nr_dlsim -n100 -R273 -b273 -P -e 25 -s30 -q 1 --loader.ldpc.shlibversion _cuda)
  add_physim_test(nr_dlsim.ldpc_cuda.1t1r_mcs25_256qam_bw "nr_dlsim: 1T1R/1 layer, MCS 25 256QAM, 40 MHz 30 kHz SCS" nr_dlsim -n100 -R106 -b106 -P -e 25 -s30 -q 1 --loader.ldpc.shlibversion _cuda)

  add_physim_test(nr_ulsim.ldpc_cuda.9seg_r13 "nr_ulsim: 9 segments, rate 1/3" nr_ulsim -n100 -s20 -m13 -R273 -r273 -W1 -y1 -z1 -P --loader.ldpc.shlibversion _cuda)
  add_physim_test(nr_ulsim.ldpc_cuda.18seg_r23 "nr_ulsim: 18 segments, rate 2/3" nr_ulsim -n100 -s20 -m22 -R273 -r273 -W1 -y1 -z1 -P --loader.ldpc.shlibversion _cuda)
  add_physim_test(nr_ulsim.ldpc_cuda.50seg_r89 "nr_ulsim: 50 segments, rate 8/9" nr_ulsim -n100 -s40 -m27 -R273 -r273 -W2 -I9 -y2 -z2 -P --loader.ldpc.shlibversion _cuda)

endif()
