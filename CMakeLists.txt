cmake_minimum_required(VERSION 3.17)

project(margen_godot VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif()

set(MARGEN_ROOT "${CMAKE_SOURCE_DIR}/../margen" CACHE PATH "Path to margen repo root")
set(MARGEN_FFI_PROFILE "release" CACHE STRING "margen_ffi cargo profile (debug/release)")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(MARGEN_FFI_PROFILE "debug")
endif()

set(MARGEN_FFI_DIR "${MARGEN_ROOT}/target/${MARGEN_FFI_PROFILE}")
find_library(
	MARGEN_FFI_LIB
	NAMES margen_ffi
	PATHS "${MARGEN_FFI_DIR}"
	REQUIRED
	NO_DEFAULT_PATH
)
message(STATUS "Using margen_ffi from ${MARGEN_FFI_LIB}")

set(GODOT_CPP_SYSTEM_HEADERS ON CACHE BOOL "" FORCE)
add_subdirectory(godot-cpp)

set(TARGET_NAME "margen_godot")

add_library(${TARGET_NAME} SHARED
	src/register_types.cpp
	src/margen_world_mesh.cpp
)

target_include_directories(${TARGET_NAME} PRIVATE
	src
	"${MARGEN_ROOT}/include"
)

target_link_libraries(${TARGET_NAME} PRIVATE
	godot-cpp
	${MARGEN_FFI_LIB}
	dl
	m
	pthread
)

get_target_property(GODOTCPP_SUFFIX godot-cpp GODOTCPP_SUFFIX)

set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

set_target_properties(
	${TARGET_NAME}
	PROPERTIES
		CXX_STANDARD 17
		CXX_EXTENSIONS OFF
		POSITION_INDEPENDENT_CODE ON
		BUILD_RPATH_USE_ORIGIN ON
		BUILD_RPATH "\$ORIGIN"
		INSTALL_RPATH "\$ORIGIN"
		LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
		RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
		PREFIX "lib"
		OUTPUT_NAME "margen_godot${GODOTCPP_SUFFIX}"
)

# Ship margen_ffi next to the extension so $ORIGIN RPATH resolves at load time.
add_custom_command(
	TARGET ${TARGET_NAME}
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${MARGEN_FFI_LIB}"
		"${OUTPUT_DIR}/libmargen_ffi.so"
	COMMENT "Copying libmargen_ffi.so into bin/"
)
