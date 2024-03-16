if (DEFINED ENV{GPICO_PATH} AND (NOT GPICO_PATH))
	set(GPICO_PATH $ENV{GPICO_PATH})
endif ()

if (NOT GPICO_PATH)
	message(FATAL_ERROR "gpico path undefined. Set GPICO_PATH.")
endif()

set(GPICO_PATH "${GPICO_PATH}" CACHE PATH "Path to the gpico library")

get_filename_component(GPICO_PATH "${GPICO_PATH}" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}")

if (NOT EXISTS ${GPICO_PATH})
	message(FATAL_ERROR "Directory '${GPICO_PATH}' not found")
endif()

if (NOT EXISTS ${GPICO_PATH}/CMakeLists.txt)
	message(FATAL_ERROR "Directory '${GPICO_PATH}' does is missing its CMake file?")
endif()

set(GPICO_PATH ${GPICO_PATH} CACHE PATH "Path to the gpico library" FORCE)

message("Using GPICO_PATH ('${GPICO_PATH}')")
add_subdirectory(${GPICO_PATH} GPICO)
