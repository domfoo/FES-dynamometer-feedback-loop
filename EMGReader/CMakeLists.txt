cmake_minimum_required(VERSION 3.19)
project(EMGReader LANGUAGES C CXX)

# ---- C/C++ standards ----
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)
qt_standard_project_setup()

# ============================================================
# 1) Build ScienceMode C library as a separate target: smpt
# ============================================================
option(SMPT_LOW_LEVEL "Enable Low-Level module" ON)
option(SMPT_MID_LEVEL "Enable Mid-Level module" ON)
option(SMPT_DYSCOM_LEVEL "Enable Dyscom-Level module" ON)

set(SMPT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ScienceMode_Library")

file(GLOB_RECURSE SMPT_SOURCES
    "${SMPT_ROOT}/src/general/*.c"
    "${SMPT_ROOT}/src/low-level/*.c"
    "${SMPT_ROOT}/src/mid-level/*.c"
    "${SMPT_ROOT}/src/dyscom-level/*.c"
)

list(FILTER SMPT_SOURCES EXCLUDE REGEX ".*/src/general/serial_port/smpt_serial_port_.*\\.c$")

list(APPEND SMPT_SOURCES
    "${SMPT_ROOT}/src/general/serial_port/smpt_serial_port.c"
)

if(WIN32)
    list(APPEND SMPT_SOURCES "${SMPT_ROOT}/src/general/serial_port/smpt_serial_port_windows.c")
elseif(APPLE)
    list(APPEND SMPT_SOURCES "${SMPT_ROOT}/src/general/serial_port/smpt_serial_port_macos.c")
else()
    list(APPEND SMPT_SOURCES "${SMPT_ROOT}/src/general/serial_port/smpt_serial_port_linux.c")
endif()

add_library(smpt STATIC ${SMPT_SOURCES})

target_include_directories(smpt PUBLIC
    "${SMPT_ROOT}/include"
    "${SMPT_ROOT}/include/general"
    "${SMPT_ROOT}/include/general/packet"
    "${SMPT_ROOT}/include/general/packet_input_buffer"
    "${SMPT_ROOT}/include/general/packet_output_buffer"
    "${SMPT_ROOT}/include/general/serial_port"
    "${SMPT_ROOT}/include/low-level"
    "${SMPT_ROOT}/include/mid-level"
    "${SMPT_ROOT}/include/dyscom-level"
)

if(SMPT_LOW_LEVEL)
    target_compile_definitions(smpt PUBLIC SMPT_LOW_LEVEL)
endif()
if(SMPT_MID_LEVEL)
    target_compile_definitions(smpt PUBLIC SMPT_MID_LEVEL)
endif()
if(SMPT_DYSCOM_LEVEL)
    target_compile_definitions(smpt PUBLIC SMPT_DYSCOM_LEVEL)
endif()

if(NOT WIN32)
    target_link_libraries(smpt PUBLIC m)
endif()

# ============================================================
# 2) Your Qt executable (C++ only)
# ============================================================
qt_add_executable(EMGReader
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    ScienceMode_Library/include/dyscom-level/smpt_dl_client.h ScienceMode_Library/include/dyscom-level/smpt_dl_definitions.h ScienceMode_Library/include/dyscom-level/smpt_dl_definitions_data_types.h ScienceMode_Library/include/dyscom-level/smpt_dl_packet_client.h ScienceMode_Library/include/dyscom-level/smpt_dl_packet_server.h ScienceMode_Library/include/dyscom-level/smpt_dl_packet_utils.h ScienceMode_Library/include/dyscom-level/smpt_dl_packet_validity.h ScienceMode_Library/include/general/packet/smpt_packet_client.h ScienceMode_Library/include/general/packet/smpt_packet_general.h ScienceMode_Library/include/general/packet/smpt_packet_internal.h ScienceMode_Library/include/general/packet/smpt_packet_server.h ScienceMode_Library/include/general/packet/smpt_packet_utils.h ScienceMode_Library/include/general/packet/smpt_packet_validity.h ScienceMode_Library/include/general/packet_input_buffer/smpt_packet_input_buffer.h ScienceMode_Library/include/general/packet_input_buffer/smpt_packet_input_buffer_definitions.h ScienceMode_Library/include/general/packet_input_buffer/smpt_packet_input_buffer_internal.h ScienceMode_Library/include/general/packet_output_buffer/smpt_packet_output_buffer.h ScienceMode_Library/include/general/serial_port/smpt_serial_port.h ScienceMode_Library/include/general/serial_port/smpt_serial_port_linux.h ScienceMode_Library/include/general/serial_port/smpt_serial_port_macos.h ScienceMode_Library/include/general/serial_port/smpt_serial_port_windows.h ScienceMode_Library/include/general/smpt_client.h ScienceMode_Library/include/general/smpt_client_cmd_lists.h ScienceMode_Library/include/general/smpt_client_data.h ScienceMode_Library/include/general/smpt_client_power.h ScienceMode_Library/include/general/smpt_client_utils.h ScienceMode_Library/include/general/smpt_definitions.h ScienceMode_Library/include/general/smpt_definitions_data_types.h ScienceMode_Library/include/general/smpt_definitions_file_transfer.h ScienceMode_Library/include/general/smpt_definitions_internal.h ScienceMode_Library/include/general/smpt_definitions_power.h ScienceMode_Library/include/general/smpt_file.h ScienceMode_Library/include/general/smpt_messages.h ScienceMode_Library/include/general/smpt_packet_number_generator.h ScienceMode_Library/include/low-level/smpt_ll_client.h ScienceMode_Library/include/low-level/smpt_ll_definitions.h ScienceMode_Library/include/low-level/smpt_ll_definitions_data_types.h ScienceMode_Library/include/low-level/smpt_ll_messages.h ScienceMode_Library/include/low-level/smpt_ll_packet_client.h ScienceMode_Library/include/low-level/smpt_ll_packet_server.h ScienceMode_Library/include/low-level/smpt_ll_packet_validity.h ScienceMode_Library/include/mid-level/smpt_ml_client.h ScienceMode_Library/include/mid-level/smpt_ml_definitions.h ScienceMode_Library/include/mid-level/smpt_ml_definitions_data_types.h ScienceMode_Library/include/mid-level/smpt_ml_packet_client.h ScienceMode_Library/include/mid-level/smpt_ml_packet_server.h ScienceMode_Library/include/mid-level/smpt_ml_packet_utils.h ScienceMode_Library/include/mid-level/smpt_ml_packet_validity.h
    ScienceMode_Library/src/dyscom-level/smpt_dl_client.c ScienceMode_Library/src/dyscom-level/smpt_dl_definitions_data_types.c ScienceMode_Library/src/dyscom-level/smpt_dl_packet_client.c ScienceMode_Library/src/dyscom-level/smpt_dl_packet_server.c ScienceMode_Library/src/dyscom-level/smpt_dl_packet_utils.c ScienceMode_Library/src/dyscom-level/smpt_dl_packet_validity.c ScienceMode_Library/src/general/packet/smpt_packet_client.c ScienceMode_Library/src/general/packet/smpt_packet_general.c ScienceMode_Library/src/general/packet/smpt_packet_internal.c ScienceMode_Library/src/general/packet/smpt_packet_server.c ScienceMode_Library/src/general/packet/smpt_packet_utils.c ScienceMode_Library/src/general/packet/smpt_packet_validity.c ScienceMode_Library/src/general/packet_input_buffer/smpt_packet_input_buffer.c ScienceMode_Library/src/general/packet_input_buffer/smpt_packet_input_buffer_internal.c ScienceMode_Library/src/general/packet_output_buffer/smpt_packet_output_buffer.c ScienceMode_Library/src/general/serial_port/smpt_serial_port.c ScienceMode_Library/src/general/serial_port/smpt_serial_port_linux.c ScienceMode_Library/src/general/serial_port/smpt_serial_port_macos.c ScienceMode_Library/src/general/serial_port/smpt_serial_port_windows.c ScienceMode_Library/src/general/smpt_client.c ScienceMode_Library/src/general/smpt_client_cmd_lists.c ScienceMode_Library/src/general/smpt_client_data.c ScienceMode_Library/src/general/smpt_client_power.c ScienceMode_Library/src/general/smpt_client_utils.c ScienceMode_Library/src/general/smpt_definitions_data_types.c ScienceMode_Library/src/general/smpt_definitions_file_transfer.c ScienceMode_Library/src/general/smpt_definitions_power.c ScienceMode_Library/src/general/smpt_file.c ScienceMode_Library/src/general/smpt_messages.c ScienceMode_Library/src/general/smpt_packet_number_generator.c ScienceMode_Library/src/low-level/smpt_ll_client.c ScienceMode_Library/src/low-level/smpt_ll_definitions_data_types.c ScienceMode_Library/src/low-level/smpt_ll_messages.c ScienceMode_Library/src/low-level/smpt_ll_packet_client.c ScienceMode_Library/src/low-level/smpt_ll_packet_server.c ScienceMode_Library/src/low-level/smpt_ll_packet_validity.c ScienceMode_Library/src/mid-level/smpt_ml_client.c ScienceMode_Library/src/mid-level/smpt_ml_definitions_data_types.c ScienceMode_Library/src/mid-level/smpt_ml_packet_client.c ScienceMode_Library/src/mid-level/smpt_ml_packet_server.c ScienceMode_Library/src/mid-level/smpt_ml_packet_utils.c ScienceMode_Library/src/mid-level/smpt_ml_packet_validity.c
    emg_metrics.h
    emg_metrics.cpp
    emg_wave_widget.h
    emg_wave_widget.cpp
    i24session.h
    i24session.cpp
)

qt_add_translations(
    TARGETS EMGReader
    TS_FILES EMGReader_en_US.ts
)

target_link_libraries(EMGReader
    PRIVATE
        Qt::Core
        Qt::Widgets
        smpt
)

include(GNUInstallDirs)

install(TARGETS EMGReader
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET EMGReader
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
