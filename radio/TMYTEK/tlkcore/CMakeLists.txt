# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(OAI_TLKCORE_ROOT "" CACHE PATH "TLKCore C/C++ example root")

if (NOT OAI_TLKCORE_ROOT)
  message(FATAL_ERROR "OAI_TLKCORE_ROOT is required when OAI_TMYTEK_TLKCORE is enabled")
endif()

set(OAI_TLKCORE_INCLUDE_DIR "${OAI_TLKCORE_ROOT}/lib_tlkcore_cpp/include")
set(OAI_TLKCORE_LIBRARY "${OAI_TLKCORE_ROOT}/lib_tlkcore_cpp/libtlkcore_lib.so")

if (NOT EXISTS "${OAI_TLKCORE_INCLUDE_DIR}/tlkcore_lib.hpp")
  message(FATAL_ERROR "TLKCore header not found: ${OAI_TLKCORE_INCLUDE_DIR}/tlkcore_lib.hpp")
endif()
if (NOT EXISTS "${OAI_TLKCORE_LIBRARY}")
  message(FATAL_ERROR "TLKCore library not found: ${OAI_TLKCORE_LIBRARY}")
endif()

add_executable(tlkcore_fbs
  src/tlkcore_fbs.cpp
)
set_target_properties(tlkcore_fbs PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED YES
  BUILD_RPATH "${OAI_TLKCORE_ROOT}/lib_tlkcore_cpp"
  INSTALL_RPATH "${OAI_TLKCORE_ROOT}/lib_tlkcore_cpp"
)
target_include_directories(tlkcore_fbs PRIVATE ${OAI_TLKCORE_INCLUDE_DIR})
target_compile_options(tlkcore_fbs PRIVATE -Wno-comment)
target_link_libraries(tlkcore_fbs PRIVATE ${OAI_TLKCORE_LIBRARY})

install(TARGETS tlkcore_fbs RUNTIME DESTINATION bin)
install(DIRECTORY config DESTINATION share/oai/tmytek/tlkcore)
