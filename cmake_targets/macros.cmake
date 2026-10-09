# SPDX-License-Identifier: LicenseRef-CSSL-1.0

###########################################
# macros to define options as there is numerous options in oai
################################################
macro(add_boolean_option name val helpstr adddef)
  if(DEFINED ${name})
    set(value ${${name}})
  else(DEFINED ${name})
    set(value ${val})
  endif()
  set(${name} ${value} CACHE STRING "${helpstr}")
  set_property(CACHE ${name} PROPERTY TYPE BOOL)
  if (${value} AND ${adddef})
    add_definitions("-D${name}")
  endif (${value} AND ${adddef})
endmacro(add_boolean_option)

macro(add_integer_option name val helpstr)
  if(DEFINED ${name})
    set(value ${${name}})
  else(DEFINED ${name})
    set(value ${val})
  endif()
  set(${name} ${value} CACHE STRING "${helpstr}")
  add_definitions("-D${name}=${value}")
endmacro(add_integer_option)

macro(add_list_string_option name val helpstr)
  if(DEFINED ${name})
    set(value ${${name}})
  else(DEFINED ${name})
    set(value ${val})
  endif()
  set(${name} ${value} CACHE STRING "${helpstr}")
  set_property(CACHE ${name} PROPERTY STRINGS ${ARGN})
  if(NOT "${value}" STREQUAL "False")
    add_definitions("-D${name}=\"${value}\"")
  endif()
endmacro(add_list_string_option)

# this function should produce the same value as the macro MAKE_VERSION defined in the C code (file types.h)
function(make_version VERSION_VALUE)
  math(EXPR RESULT "0")
  foreach (ARG ${ARGN})
    math(EXPR RESULT "${RESULT} * 256 + ${ARG}")
  endforeach()
  set(${VERSION_VALUE} "${RESULT}" PARENT_SCOPE)
endfunction()

macro(eval_boolean VARIABLE)
  if(${ARGN})
    set(${VARIABLE} ON)
  else()
    set(${VARIABLE} OFF)
  endif()
endmacro()

# download URL into FILE, retrying on failure: a server-side hiccup makes
# file(DOWNLOAD) leave an empty file behind, which then breaks the tools
# consuming it
function(download_file URL FILE)
  cmake_parse_arguments(DL "" "RETRIES;RETRY_DELAY" "" ${ARGN})
  if(NOT DEFINED DL_RETRIES)
    set(DL_RETRIES 3)
  endif()
  if(NOT DEFINED DL_RETRY_DELAY)
    set(DL_RETRY_DELAY 1)
  endif()

  foreach(attempt RANGE 1 ${DL_RETRIES})
    file(DOWNLOAD ${URL} ${FILE} STATUS status)
    list(GET status 0 code)
    list(GET status 1 reason)
    set(size 0)
    if(EXISTS ${FILE})
      file(SIZE ${FILE} size)
    endif()
    if(code EQUAL 0 AND size GREATER 0)
      return()
    endif()
    if(code EQUAL 0)
      set(reason "downloaded file is empty")
    endif()
    message(WARNING "download of ${URL} failed (attempt ${attempt} of ${DL_RETRIES}): ${reason}")
    if(attempt LESS ${DL_RETRIES})
      execute_process(COMMAND ${CMAKE_COMMAND} -E sleep ${DL_RETRY_DELAY})
    endif()
  endforeach()
  message(FATAL_ERROR "could not download ${URL} in ${DL_RETRIES} attempts: ${reason}")
endfunction()

function(check_option EXEC TEST_OPTION EXEC_HINT)
  message(STATUS "Check if ${EXEC} supports ${TEST_OPTION}")
  execute_process(COMMAND ${EXEC} ${TEST_OPTION}
                  RESULT_VARIABLE CHECK_STATUS
                  OUTPUT_VARIABLE CHECK_OUTPUT
                  ERROR_VARIABLE CHECK_OUTPUT)
  if(NOT ${CHECK_STATUS} EQUAL 1)
    get_filename_component(EXEC_FILE ${EXEC} NAME)
    message(FATAL_ERROR "Error message: ${CHECK_OUTPUT}\
You might want to re-run ./build_oai -I
Or provide a path to ${EXEC_FILE} using
  ./build_oai ... --cmake-opt -D${EXEC_HINT}=/path/to/${EXEC_FILE}
or directly with
  cmake .. -D${EXEC_HINT}=/path/to/${EXEC_FILE}
")
  endif()
endfunction()

function(run_asn1c ASN1C_GRAMMAR ASN1C_PREFIX)
  set(options "")
  set(oneValueArgs COMMENT)
  set(multiValueArgs OUTPUT OPTIONS)
  cmake_parse_arguments(ASN1C "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  get_filename_component(GRAMMAR_FILE ${ASN1C_GRAMMAR} NAME)
  set(LOGFILE "${CMAKE_CURRENT_BINARY_DIR}/${GRAMMAR_FILE}.log")
  add_custom_command(OUTPUT ${ASN1C_OUTPUT}
    COMMAND ASN1C_PREFIX=${ASN1C_PREFIX} ${ASN1C_EXEC} ${ASN1C_OPTIONS} -D ${CMAKE_CURRENT_BINARY_DIR} ${ASN1C_GRAMMAR} > ${LOGFILE} 2>&1 || ( cat ${LOGFILE} && false )
    DEPENDS ${ASN1C_GRAMMAR} asn1c
    COMMENT "Generating ${ASN1C_COMMENT} from ${GRAMMAR_FILE}"
  )
endfunction()

define_property(TEST PROPERTY TEST_DESCRIPTION
                BRIEF_DOCS "A human-readable description of this test"
                FULL_DOCS "A human-readable description of this test")

define_property(TEST PROPERTY CHECK_COUNT
                BRIEF_DOCS "helper property to enumerate checks in environment"
                FULL_DOCS "the property counts the number of threshold checks, used to enumerate environment variables given to analyze-timing.sh")

function(add_physim_test test_name test_description test_exec)
  # catch all the arguments past the last expected arqument and store them in the options_list
  if (NOT TARGET ${test_exec})
    message(FATAL_ERROR "test executable ${test_exec} is not an executable")
  endif()
  set(test_invocation $<TARGET_FILE:${test_exec}> ${ARGN})
  add_test(
    NAME ${test_name}
    COMMAND ${CMAKE_COMMAND}
        "-DTEST_CMD=${test_invocation}"
        "-DCHECK_SCRIPT=${CMAKE_SOURCE_DIR}/openair1/SIMULATION/tests/analyze-timing.sh"
        -P ${CMAKE_SOURCE_DIR}/openair1/SIMULATION/tests/RunTimedTest.cmake
  )
  set_tests_properties(${test_name} PROPERTIES
    LABELS "${test_exec}"
    TEST_DESCRIPTION "${test_description}"
    # pass test description also through environment variable: for cmake < 3.30,
    # in JSON export, we cannot recover the description otherwise
    # see also https://gitlab.kitware.com/cmake/cmake/-/issues/21490
    ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR};TEST_DESCRIPTION=${test_description}"
  )
  set_tests_properties(${test_name} PROPERTIES CHECK_COUNT 0)
  add_dependencies(tests ${test_exec})
endfunction()

function(check_threshold testname threshold condition)
  # check that threshold and condition don't have a colon (;), because that
  # would interfere with cmake's list management
  string(FIND "${threshold}" ";" pos)
  if (pos GREATER -1)
    message(FATAL_ERROR "colon not allowed in threshold, but have \"${threshold}\"")
  endif()
  string(FIND "${condition}" ";" pos)
  if (pos GREATER -1)
    message(FATAL_ERROR "colon not allowed in condition, but have \"${condition}\"")
  endif()
  set(THRCOND "${threshold}\;${condition}")

  get_test_property(${testname} CHECK_COUNT count)
  #message(STATUS "add check ${count} ${THRCOND}")
  if (${count} GREATER 10)
    message(FATAL_ERROR "only maximum of 10 checks per test allowed")
  endif()

  # add a new environment variable CHECK_X with this threshold+condition, then
  # increase test property regarding check count
  set_property(TEST ${testname} APPEND PROPERTY ENVIRONMENT "CHECK_${count}=${THRCOND}")
  MATH(EXPR count "${count}+1")
  set_tests_properties(${testname} PROPERTIES CHECK_COUNT ${count})
endfunction()

function(check_threshold_range testname threshold)
  cmake_parse_arguments(RANGE "" "LOWER;UPPER" "" ${ARGN})
  if (NOT RANGE_LOWER AND NOT RANGE_UPPER)
    message(FATAL_ERROR "need at least one LOWER or one UPPER threshold")
  endif()
  if (RANGE_LOWER)
    check_threshold(${testname} ${threshold} "> ${RANGE_LOWER}")
  endif()
  if (RANGE_UPPER)
    check_threshold(${testname} ${threshold} "< ${RANGE_UPPER}")
  endif()
endfunction()

function(check_threshold_variance testname threshold)
  cmake_parse_arguments(VARIANCE "" "AVG;ABS_VAR" "" ${ARGN})
  if (NOT VARIANCE_AVG AND NOT VARIANCE_ABS_VAR)
    message(FATAL_ERROR "need both AVG and ABS_VAR")
  endif()
  MATH(EXPR upper "${VARIANCE_AVG}+${VARIANCE_ABS_VAR}")
  MATH(EXPR lower "${VARIANCE_AVG}-${VARIANCE_ABS_VAR}")
  check_threshold_range(${testname} ${threshold} LOWER ${lower} UPPER ${upper})
endfunction()
