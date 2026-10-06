# clang-cl + xwin CRT/SDK toolchain for Linux → Windows x86_64 MSVC GDExtension builds.
# Expects XWIN_CACHE_DIR (default /opt/cargo-xwin). cargo-xwin stores the splat at
# ${XWIN_CACHE_DIR}/xwin/{crt,sdk}. Helper tools (clang-cl, lld-link, llvm-lib) on PATH.
# Rebuild the marloth-win image if the splat is missing — do not download at build time.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if(NOT DEFINED ENV{XWIN_CACHE_DIR} OR "$ENV{XWIN_CACHE_DIR}" STREQUAL "")
	set(_XWIN_BASE "/opt/cargo-xwin")
else()
	set(_XWIN_BASE "$ENV{XWIN_CACHE_DIR}")
endif()

# cargo-xwin joins "xwin" under XWIN_CACHE_DIR; also accept a direct splat path.
if(EXISTS "${_XWIN_BASE}/xwin/crt" AND EXISTS "${_XWIN_BASE}/xwin/sdk")
	set(_XWIN_DIR "${_XWIN_BASE}/xwin")
elseif(EXISTS "${_XWIN_BASE}/crt" AND EXISTS "${_XWIN_BASE}/sdk")
	set(_XWIN_DIR "${_XWIN_BASE}")
else()
	message(FATAL_ERROR
		"Xwin CRT/SDK missing under ${_XWIN_BASE} (expected ${_XWIN_BASE}/xwin/crt). "
		"Rebuild the marloth-win image (do not download CRT/SDK at build time).")
endif()

set(CMAKE_C_COMPILER clang-cl CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER clang-cl CACHE FILEPATH "")
set(CMAKE_AR llvm-lib)
set(CMAKE_LINKER lld-link CACHE FILEPATH "")
set(CMAKE_RC_COMPILER llvm-rc CACHE FILEPATH "")
set(CMAKE_MT ":" CACHE FILEPATH "Disable mt.exe; not required for this cross-build")

# MSVC-style import libraries when find_library runs on Linux hosts.
set(CMAKE_FIND_LIBRARY_PREFIXES "")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".dll.lib" ".lib")

set(_TARGET_TRIPLE "x86_64-pc-windows-msvc")
set(_XWIN_ARCH "x86_64")

set(COMPILE_FLAGS
	--target=${_TARGET_TRIPLE}
	-Wno-unused-command-line-argument
	-fuse-ld=lld-link
	/imsvc${_XWIN_DIR}/crt/include
	/imsvc${_XWIN_DIR}/sdk/include/ucrt
	/imsvc${_XWIN_DIR}/sdk/include/um
	/imsvc${_XWIN_DIR}/sdk/include/shared
)

set(LINK_FLAGS
	/manifest:no
	-libpath:"${_XWIN_DIR}/crt/lib/${_XWIN_ARCH}"
	-libpath:"${_XWIN_DIR}/sdk/lib/um/${_XWIN_ARCH}"
	-libpath:"${_XWIN_DIR}/sdk/lib/ucrt/${_XWIN_ARCH}"
)

string(REPLACE ";" " " COMPILE_FLAGS "${COMPILE_FLAGS}")
string(REPLACE ";" " " LINK_FLAGS "${LINK_FLAGS}")

set(_CMAKE_C_FLAGS_INITIAL "${CMAKE_C_FLAGS}" CACHE STRING "")
set(CMAKE_C_FLAGS "${_CMAKE_C_FLAGS_INITIAL} ${COMPILE_FLAGS}" CACHE STRING "" FORCE)

set(_CMAKE_CXX_FLAGS_INITIAL "${CMAKE_CXX_FLAGS}" CACHE STRING "")
set(CMAKE_CXX_FLAGS "${_CMAKE_CXX_FLAGS_INITIAL} ${COMPILE_FLAGS}" CACHE STRING "" FORCE)

set(_CMAKE_EXE_LINKER_FLAGS_INITIAL "${CMAKE_EXE_LINKER_FLAGS}" CACHE STRING "")
set(CMAKE_EXE_LINKER_FLAGS "${_CMAKE_EXE_LINKER_FLAGS_INITIAL} ${LINK_FLAGS}" CACHE STRING "" FORCE)

set(_CMAKE_MODULE_LINKER_FLAGS_INITIAL "${CMAKE_MODULE_LINKER_FLAGS}" CACHE STRING "")
set(CMAKE_MODULE_LINKER_FLAGS "${_CMAKE_MODULE_LINKER_FLAGS_INITIAL} ${LINK_FLAGS}" CACHE STRING "" FORCE)

set(_CMAKE_SHARED_LINKER_FLAGS_INITIAL "${CMAKE_SHARED_LINKER_FLAGS}" CACHE STRING "")
set(CMAKE_SHARED_LINKER_FLAGS "${_CMAKE_SHARED_LINKER_FLAGS_INITIAL} ${LINK_FLAGS}" CACHE STRING "" FORCE)

# Avoid CMake injecting host/default Windows libs that need case-correcting symlinks.
set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)

# Match godot-cpp GODOTCPP_USE_STATIC_CPP=ON with GODOTCPP_DEBUG_CRT=OFF:
# static release CRT (/MT) even for Debug configs — avoids needing libcmtd from xwin.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "" FORCE)

set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
