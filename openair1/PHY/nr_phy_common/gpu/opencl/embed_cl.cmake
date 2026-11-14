# SPDX-License-Identifier: LicenseRef-CSSL-1.0
# cmake -DOUT=<file.c> -DNAME=<symbol> -DIN=<a|b|...> -P embed_cl.cmake: the files concatenated into a NUL-terminated
# const char array (the OpenCL program source of an offload module)
string(REPLACE "|" ";" IN "${IN}")
file(WRITE ${OUT} "const char ${NAME}[] = {\n")
foreach(f ${IN})
  file(READ ${f} hex HEX)
  string(REGEX REPLACE "(..)" "0x\\1," hex "${hex}")
  file(APPEND ${OUT} "${hex}\n")
endforeach()
file(APPEND ${OUT} "0};\n")
