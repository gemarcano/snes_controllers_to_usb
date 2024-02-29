cmake_minimum_required(VERSION 3.20)

include(pico_sdk_import.cmake)
include(FreeRTOS_Kernel_import.cmake)

project(snes_controllers_to_usb)

pico_sdk_init()

add_executable(snes_controllers_to_usb
	src/main.cpp
	src/log.cpp
	src/cli_task.cpp
	src/FreeRTOS_support.cpp
	src/usb_descriptors.cpp
	src/syscalls.cpp
	src/watchdog.cpp
)

pico_generate_pio_header(snes_controllers_to_usb
	${CMAKE_CURRENT_LIST_DIR}/src/controllers.pio
)

target_link_libraries(snes_controllers_to_usb
	pico_stdlib
	pico_rand
	FreeRTOS-Kernel-Heap4
	tinyusb_device
	tinyusb_board
	hardware_pio
)

target_compile_options(snes_controllers_to_usb PRIVATE
	$<$<CXX_COMPILER_ID:MSVC>:/W4>
	$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
	-fstack-usage
	-Wno-psabi
)

target_compile_definitions(snes_controllers_to_usb PRIVATE
	PICO_DEFAULT_UART_TX_PIN=18
	PICO_DEFAULT_UART_RX_PIN=19
)

target_include_directories(snes_controllers_to_usb PRIVATE
	${CMAKE_SOURCE_DIR}/include
)

target_compile_features(snes_controllers_to_usb PRIVATE
	cxx_std_23
)

pico_set_printf_implementation(snes_controllers_to_usb compiler)
pico_set_stdio_implementation(snes_controllers_to_usb compiler)

set_property(TARGET snes_controllers_to_usb PROPERTY CXX_STANDARD 23)

if (DEFINED HOSTNAME)
	add_compile_definitions(CYW43_HOST_NAME=\"${HOSTNAME}\")
endif()

pico_add_extra_outputs(snes_controllers_to_usb)
