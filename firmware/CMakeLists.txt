cmake_minimum_required(VERSION 3.20)

include(pico_sdk_import.cmake)
include(FreeRTOS_Kernel_import.cmake)

project(snes_controllers_to_usb)

pico_sdk_init()

add_executable(snes_controllers_to_usb
	src/main.cpp
	src/log.cpp
	src/server.cpp
	src/network_task.cpp
	src/cli_task.cpp
	src/FreeRTOS_support.cpp
	src/usb_descriptors.cpp
	src/syscalls.cpp
	src/wifi_management_task.cpp
	src/watchdog.cpp
)

target_link_libraries(snes_controllers_to_usb
	pico_cyw43_arch_lwip_sys_freertos
	pico_stdlib
	pico_rand
	FreeRTOS-Kernel-Heap3
	tinyusb_device
	tinyusb_board
)

target_compile_options(snes_controllers_to_usb PRIVATE
	$<$<CXX_COMPILER_ID:MSVC>:/W4>
	$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
	-fstack-usage
	-Wno-psabi
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
