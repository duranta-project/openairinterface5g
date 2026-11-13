if(NOT CROSS_COMPILE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  add_library(cpu_features OBJECT get_enabled_instructions.c cpu_check.c)
  set_source_files_properties(cpu_check.c
    PROPERTIES COMPILE_OPTIONS "-march=x86-64;-mno-avx;-mno-bmi;-mno-bmi2;-mno-lzcnt;-mno-movbe")
else()
  add_library(cpu_features INTERFACE)
endif()
