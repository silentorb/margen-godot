cmake_minimum_required(VERSION 3.21)

project(margen_godot
    VERSION 0.1.0
    DESCRIPTION "Godot GDExtension host for margen"
    LANGUAGES CXX
)

# Placeholder: godot-cpp and the shared library target land when the first
# mesh/scene conversion from margen datasets is implemented.
#
# Expected later shape:
#   # Build/install margen-ffi (Rust) and point at include/margen.h + libmargen_ffi
#   add_subdirectory(godot-cpp)  # or FetchContent
#   add_library(margen_godot SHARED ...)
#   target_include_directories(margen_godot PRIVATE ${MARGEN_INCLUDE_DIR})
#   target_link_libraries(margen_godot PRIVATE margen_ffi godot-cpp)

message(STATUS "margen-godot: scaffold only — no GDExtension targets yet")
