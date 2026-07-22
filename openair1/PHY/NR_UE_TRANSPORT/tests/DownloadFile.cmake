# Called with: cmake -DURL=... -DDEST=... -P DownloadFile.cmake
get_filename_component(dest_dir "${DEST}" DIRECTORY)
file(MAKE_DIRECTORY "${dest_dir}")

message(STATUS "Downloading ${URL} -> ${DEST}")
file(DOWNLOAD "${URL}" "${DEST}"
    SHOW_PROGRESS
    STATUS status
)

list(GET status 0 code)
list(GET status 1 error_msg)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "Download failed (${code}): ${error_msg}")
endif()
message(STATUS "Download complete")
