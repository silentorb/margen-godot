# Agent notes — margen-godot

## Project

**margen-godot** is the Godot **GDExtension** host for [margen](https://github.com/). It converts margen output datasets into Godot entities (meshes, nodes, materials).

- **Do not** put world-generation algorithms here — those live in **margen** (Rust).
- **Do not** put general utilities here — those live in margen’s **mythic** crate (or a future standalone mythic repo).

## Current status

Scaffold only. `godot-cpp` and a `.gdextension` resource land when the first mesh/scene conversion is needed.

## Relation to other repos

| Repo | Role |
|------|------|
| **margen** | Engine-agnostic world generation (Rust) + mythic utilities + **C ABI** (`include/margen.h`) |
| **margen-godot** | This repo — Godot integration via the C ABI |
| **marloth** | Godot game that will consume the extension (game glue likely C#) |

## Build (future)

Expected shape once wired:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Link against **margen-ffi** (`libmargen_ffi` / `margen_ffi`) and include margen’s [`include/margen.h`](../margen/include/margen.h)—not a C++ `margen::margen` CMake package.

## Conventions

- **Line endings:** Unix (LF). See [`.gitattributes`](.gitattributes) and [`.editorconfig`](.editorconfig).
- GDExtension sources remain C++ (godot-cpp); consume margen only through the **C ABI**.
- Algorithm style lives in margen’s [docs/rust-style.md](../margen/docs/rust-style.md).
