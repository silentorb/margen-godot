# Agent notes — margen-godot

## Project

**margen-godot** is the Godot **GDExtension** host for [margen](https://github.com/). It converts margen output datasets into Godot entities (meshes, nodes, materials).

- **Do not** put world-generation algorithms here — those live in **margen** (Rust).
- **Do not** put general utilities here — those live in margen’s **mythic** crate (or a future standalone mythic repo).

## Current status

`MargenWorldMesh` converts `margen_generate_world_faces` output into an `ArrayMesh` with placeholder `StandardMaterial3D` per used slot. Linux debug/release builds are supported first.

## Relation to other repos

| Repo | Role |
|------|------|
| **margen** | Engine-agnostic world generation (Rust) + mythic utilities + **C ABI** (`include/margen.h`) |
| **margen-godot** | This repo — Godot integration via the C ABI |
| **marloth** | Godot game that loads the extension and hosts debug/integration scenes |

## Build

**Dev container (recommended):** Reopen this repo in [`.devcontainer/`](.devcontainer/) — Rust, Python 3, CMake, Ninja, and `build-essential` live here so marloth/margen images stay lean. The container mounts sibling **`margen`** at `/workspaces/margen`.

```bash
git submodule update --init --recursive   # also runs as postCreateCommand
./scripts/build.sh
./scripts/install-to-marloth.sh   # optional: copy .so into marloth addons/
```

Link against **margen-ffi** (`libmargen_ffi`) and include margen’s [`include/margen.h`](../margen/include/margen.h).

## Conventions

- **Line endings:** Unix (LF). See [`.gitattributes`](.gitattributes) and [`.editorconfig`](.editorconfig).
- GDExtension sources remain C++ (godot-cpp); consume margen only through the **C ABI**.
- Algorithm style lives in margen’s [docs/rust-style.md](../margen/docs/rust-style.md).
