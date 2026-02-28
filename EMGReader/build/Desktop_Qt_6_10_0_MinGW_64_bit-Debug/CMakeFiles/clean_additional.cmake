# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\EMGReader_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\EMGReader_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\smpt_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\smpt_autogen.dir\\ParseCache.txt"
  "EMGReader_autogen"
  "smpt_autogen"
  )
endif()
