cmake_minimum_required(VERSION 3.17)

project(margen_godot VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif()

set(MARGEN_ROOT "${CMAKE_SOURCE_DIR}/../margen" CACHE PATH "Path to margen repo root")
set(MARGEN_FFI_PROFILE "release" CACHE STRING "margen_ffi cargo profile (debug/release)")
set(MARGEN_FFI_TARGET "" CACHE STRING "Optional cargo target triple (e.g. x86_64-pc-windows-msvc)")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(MARGEN_FFI_PROFILE "debug")
endif()

# Honor CARGO_TARGET_DIR when set (marloth-win isolates from bind-mounted margen/target).
set(_MARGEN_CARGO_TARGET_ROOT "${MARGEN_ROOT}/target")
if(DEFINED ENV{CARGO_TARGET_DIR} AND NOT "$ENV{CARGO_TARGET_DIR}" STREQUAL "")
	set(_MARGEN_CARGO_TARGET_ROOT "$ENV{CARGO_TARGET_DIR}")
endif()

if(MARGEN_FFI_TARGET STREQUAL "")
	set(MARGEN_FFI_DIR "${_MARGEN_CARGO_TARGET_ROOT}/${MARGEN_FFI_PROFILE}")
else()
	set(MARGEN_FFI_DIR "${_MARGEN_CARGO_TARGET_ROOT}/${MARGEN_FFI_TARGET}/${MARGEN_FFI_PROFILE}")
endif()

# Linux: libmargen_ffi.so / libmargen_ffi.a. MSVC: margen_ffi.dll.lib (prefer) or margen_ffi.lib.
# Drop cached find result so a changed CARGO_TARGET_DIR is picked up on reconfigure.
unset(MARGEN_FFI_LIB CACHE)
find_library(
	MARGEN_FFI_LIB
	NAMES margen_ffi.dll margen_ffi
	PATHS "${MARGEN_FFI_DIR}"
	REQUIRED
	NO_DEFAULT_PATH
	NO_CMAKE_FIND_ROOT_PATH
)
message(STATUS "Using margen_ffi from ${MARGEN_FFI_LIB}")

# Stamp the godot-cpp extension_api.json header version into the binary for diagnostics.
set(_MARGEN_API_JSON "${CMAKE_SOURCE_DIR}/godot-cpp/gdextension/extension_api.json")
set(MARGEN_GODOT_API_VERSION "unknown")
if(EXISTS "${_MARGEN_API_JSON}")
	file(STRINGS "${_MARGEN_API_JSON}" _api_major REGEX "\"version_major\"")
	file(STRINGS "${_MARGEN_API_JSON}" _api_minor REGEX "\"version_minor\"")
	file(STRINGS "${_MARGEN_API_JSON}" _api_patch REGEX "\"version_patch\"")
	string(REGEX REPLACE ".*: *([0-9]+).*" "\\1" _api_major_n "${_api_major}")
	string(REGEX REPLACE ".*: *([0-9]+).*" "\\1" _api_minor_n "${_api_minor}")
	string(REGEX REPLACE ".*: *([0-9]+).*" "\\1" _api_patch_n "${_api_patch}")
	if(_api_major_n AND _api_minor_n AND NOT _api_patch_n STREQUAL "")
		set(MARGEN_GODOT_API_VERSION "${_api_major_n}.${_api_minor_n}.${_api_patch_n}")
	endif()
endif()
message(STATUS "margen_godot API version stamp: ${MARGEN_GODOT_API_VERSION}")

set(GODOT_CPP_SYSTEM_HEADERS ON CACHE BOOL "" FORCE)
add_subdirectory(godot-cpp)

set(TARGET_NAME "margen_godot")

add_library(${TARGET_NAME} SHARED
	src/register_types.cpp
	src/margen_world_mesh.cpp
)

target_compile_definitions(${TARGET_NAME} PRIVATE
	"MARGEN_GODOT_API_VERSION=\"${MARGEN_GODOT_API_VERSION}\""
)

target_include_directories(${TARGET_NAME} PRIVATE
	src
	"${MARGEN_ROOT}/include"
)

target_link_libraries(${TARGET_NAME} PRIVATE
	godot-cpp
	${MARGEN_FFI_LIB}
)

if(NOT WIN32)
	target_link_libraries(${TARGET_NAME} PRIVATE
		dl
		m
		pthread
	)
endif()

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

# Ship margen_ffi next to the extension so $ORIGIN / same-dir load resolves at runtime.
if(WIN32)
	set(_MARGEN_FFI_RUNTIME "${MARGEN_FFI_DIR}/margen_ffi.dll")
	if(NOT EXISTS "${_MARGEN_FFI_RUNTIME}")
		message(FATAL_ERROR "Expected Windows DLL at ${_MARGEN_FFI_RUNTIME}")
	endif()
	set(_MARGEN_FFI_DEST "${OUTPUT_DIR}/margen_ffi.dll")
else()
	set(_MARGEN_FFI_RUNTIME "${MARGEN_FFI_LIB}")
	set(_MARGEN_FFI_DEST "${OUTPUT_DIR}/libmargen_ffi.so")
endif()

add_custom_command(
	TARGET ${TARGET_NAME}
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${_MARGEN_FFI_RUNTIME}"
		"${_MARGEN_FFI_DEST}"
	COMMENT "Copying margen_ffi into bin/"
)
