# Agent notes — margen-godot

## Project

**margen-godot** is the Godot **GDExtension** host for [margen](https://github.com/). It converts margen output datasets into Godot entities (meshes, nodes, materials).

- **Do not** put world-generation algorithms here — those live in **margen**.
- **Do not** put general utilities here — those live in margen’s **mythic** layer (or a future standalone mythic repo).

## Current status

Scaffold only. `godot-cpp` and a `.gdextension` resource land when the first mesh/scene conversion is needed.

## Relation to other repos

| Repo | Role |
|------|------|
| **margen** | Engine-agnostic world generation + mythic utilities |
| **margen-godot** | This repo — Godot integration |
| **marloth** | Godot game that will consume the extension |

## Build (future)

Expected shape once wired:

```bash
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Link against `margen::margen` via `find_package(margen)` or `add_subdirectory` of the margen tree.

## Conventions

- **Line endings:** Unix (LF). See [`.gitattributes`](.gitattributes) and [`.editorconfig`](.editorconfig).
- **Headers:** use `.h` (not `.hpp`) for C++ headers, matching margen.
- **Colocated headers and sources:** Keep `.h` and `.cpp` in the same directories (no separate `include/` tree), matching margen.
- **C++ style:** Follow margen’s [docs/cpp-style.md](../margen/docs/cpp-style.md) (shared workspace guide).
