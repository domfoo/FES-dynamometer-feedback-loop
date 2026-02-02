cmake_minimum_required(VERSION 3.20)
project(FESController C)

set(CMAKE_C_STANDARD 99)

# 1. Gather all your Source files
# This matches your old logic: explicit files + everything in src/
file(GLOB_RECURSE SRC_FILES 
    "src/*.c"
    "ems.c"
    "main.c"
    "main_copy.c" 
    "main_copy2.c"
)

# 2. Gather Header files (for IDE navigation)
file(GLOB_RECURSE HEADER_FILES "src/*.h")

# 3. Create the Executable
add_executable(FESController ${SRC_FILES} ${HEADER_FILES})

# 4. Include Directories (Where to look for .h files)
target_include_directories(FESController PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 5. Link the library (from your DISTFILES entry)
# Note: In your .pro file, this was in DISTFILES, not LIBS. 
# If you get linker errors, uncomment the line below:
# target_link_libraries(FESController PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lib/libsmpt.lib")