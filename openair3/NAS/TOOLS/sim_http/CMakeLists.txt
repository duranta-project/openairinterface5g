# SPDX-License-Identifier: LicenseRef-CSSL-1.0

find_package(CURL QUIET)
if (NOT CURL_FOUND)
  find_path(CURL_INCLUDE_DIR NAMES curl/curl.h)
  find_library(CURL_LIBRARY NAMES curl)
  if (CURL_INCLUDE_DIR AND CURL_LIBRARY)
    add_library(CURL::libcurl INTERFACE IMPORTED)
    set_target_properties(CURL::libcurl PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "${CURL_LIBRARY}")
    set(CURL_FOUND TRUE)
  endif()
endif()

if (NOT CURL_FOUND)
  message(FATAL_ERROR "libcurl not found. Install libcurl4-openssl-dev (Debian/Ubuntu) or libcurl-devel (Fedora).")
endif()

add_library(sim_http OBJECT
    cJSON.c
    curl_http_client.c
    sim_http_auth.c
)
target_include_directories(sim_http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sim_http PUBLIC CURL::libcurl UTIL)
