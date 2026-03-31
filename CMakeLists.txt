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

# Default to Pico W unless overridden
if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico_w CACHE STRING "Target board")
endif()

project(hid-pico-a C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

add_executable(${PROJECT_NAME}
    main.c
    led.c
    hid_transport.c
    usb_descriptors.c
    command_interface.c
    uart_interface.c
    i2c_interface.c
    lux.c
)

# Example include
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Pull in our pico_stdlib which pulls in commonly used features
target_link_libraries(${PROJECT_NAME} pico_stdlib tinyusb_device tinyusb_board pico_unique_id pico_cyw43_arch_none hardware_i2c)

# Enable USB output, disable UART output
# pico_enable_stdio_usb(${PROJECT_NAME} 1)
# pico_enable_stdio_uart(${PROJECT_NAME} 0)

# Create map/bin/hex file etc.
pico_add_extra_outputs(${PROJECT_NAME})