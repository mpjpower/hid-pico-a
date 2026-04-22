# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

# List of supported boards
set(SUPPORTED_BOARDS pico pico_w pico2 pico2_w)

# Default to Pico W unless overridden
if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico_w CACHE STRING "Target board: pico_w, pico2, or pico2_w")
endif()

# Validate board choice
if(NOT PICO_BOARD IN_LIST SUPPORTED_BOARDS)
    message(FATAL_ERROR "PICO_BOARD=${PICO_BOARD} not supported. Choose from: ${SUPPORTED_BOARDS}")
endif()

project(hid-pico-a C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# Define which boards have wireless support
set(WIRELESS_BOARDS pico_w pico2_w)

if(PICO_BOARD IN_LIST WIRELESS_BOARDS)
    set(HAS_WIRELESS TRUE)
else()
    set(HAS_WIRELESS FALSE)
endif()

# Create executable with board-specific suffix
set(TARGET_NAME "${PROJECT_NAME}-${PICO_BOARD}")
add_executable(${TARGET_NAME}
    src/main.c
    src/led.c
    src/hid_transport.c
    src/usb_descriptors.c
    src/command_interface.c
    src/uart_interface.c
    src/i2c_interface.c
    src/lux.c
)

# Include directories
target_include_directories(${TARGET_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Conditionally link wireless libraries and set preprocessor definition
if(HAS_WIRELESS)
    set(WIRELESS_LIBS pico_cyw43_arch_none)
    target_compile_definitions(${TARGET_NAME} PRIVATE PICO_BOARD_HAS_WIRELESS=1)
else()
    set(WIRELESS_LIBS "")
    target_compile_definitions(${TARGET_NAME} PRIVATE PICO_BOARD_HAS_WIRELESS=0)
endif()

# Link libraries
target_link_libraries(${TARGET_NAME} 
    pico_stdlib 
    tinyusb_device 
    tinyusb_board 
    pico_unique_id 
    ${WIRELESS_LIBS}
    hardware_i2c
)

# Enable USB output, disable UART output
# pico_enable_stdio_usb(${TARGET_NAME} 1)
# pico_enable_stdio_uart(${TARGET_NAME} 0)

# Create map/bin/hex file etc.
pico_add_extra_outputs(${TARGET_NAME})