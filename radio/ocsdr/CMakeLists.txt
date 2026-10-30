# OC1 board with our own FPGA image, through libocsdr. OCSDR_DIR is an ocsdr checkout built in OCSDR_DIR/build.
set(OCSDR_DIR "$ENV{HOME}/ocsdr" CACHE PATH "ocsdr checkout with libocsdr built in build/")
find_library(OCSDR_LIB ocsdr PATHS ${OCSDR_DIR}/build NO_DEFAULT_PATH REQUIRED)
find_library(OCSDR_NOOS_LIB noos PATHS ${OCSDR_DIR}/build NO_DEFAULT_PATH REQUIRED)

add_library(oai_ocsdr MODULE ocsdr_lib.cpp)
target_include_directories(oai_ocsdr PRIVATE ${OCSDR_DIR}/include)
target_link_libraries(oai_ocsdr PRIVATE UTIL ${OCSDR_LIB} ${OCSDR_NOOS_LIB} m)
set_target_properties(oai_ocsdr PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
