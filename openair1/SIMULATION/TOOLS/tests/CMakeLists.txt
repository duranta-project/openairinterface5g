# SPDX-License-Identifier: LicenseRef-CSSL-1.0
add_library(gaussian_scalar_reference OBJECT ../gaussian_rng.c)
target_compile_definitions(gaussian_scalar_reference PRIVATE
  GAUSSIAN_RNG_FORCE_SCALAR
  gaussian_rng_init=scalar_gaussian_rng_init
  gaussian_rng_sample=scalar_gaussian_rng_sample
  gaussian_rng_fill=scalar_gaussian_rng_fill
  gaussian_rng_fill_float=scalar_gaussian_rng_fill_float
  add_noise_float_batch=scalar_add_noise_float_batch)
target_compile_options(gaussian_scalar_reference PRIVATE -ffp-contract=off)
foreach(name test_gaussian_rng test_gaussian_batch_rng)
  add_executable(${name} ${name}.c $<TARGET_OBJECTS:gaussian_scalar_reference>)
  target_link_libraries(${name} PRIVATE SIMU minimal_lib m pthread)
  add_dependencies(tests ${name})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES TIMEOUT 120)
endforeach()
