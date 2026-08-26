# Agent notes — margen-godot

## Project

**margen-godot** is the Godot **GDExtension** host for [margen](https://github.com/). It converts margen output datasets into Godot entities (meshes, nodes, materials).

- **Do not** put world-generation algorithms here — those live in **margen** (Rust).
- **Do not** put general utilities here — those live in margen’s **mythic** crate (or a future standalone mythic repo).

## Current status

`MargenWorldMesh` converts `margen_generate_world_faces` output into an `ArrayMesh` with placeholder `StandardMaterial3D` per used slot. Linux and Windows (MinGW cross from Linux) debug/release builds are supported.

## Relation to other repos

| Repo | Role |
|------|------|
| **margen** | Engine-agnostic world generation (Rust) + mythic utilities + **C ABI** (`include/margen.h`) |
| **margen-godot** | This repo — Godot integration via the C ABI |
| **marloth** | Godot game that loads the extension and hosts debug/integration scenes |

## Build

**Dev container (recommended):** Reopen this repo in [`.devcontainer/`](.devcontainer/) — Rust, Python 3, CMake, Ninja, and `build-essential` live here so marloth/margen images stay lean. The container mounts sibling **`margen`** at `/workspaces/margen`. For Windows natives, prefer Marloth’s **`marloth-win`** compose service (MinGW + Rust `windows-gnu`).

```bash
git submodule update --init --recursive   # also runs as postCreateCommand
./scripts/build.sh                        # Linux .so
TARGET=windows ./scripts/build.sh         # Windows .dll (needs MinGW toolchain)
./scripts/install-to-marloth.sh           # optional: copy natives into marloth addons/
```

Link against **margen-ffi** (`libmargen_ffi` / `margen_ffi.dll`) and include margen’s [`include/margen.h`](../margen/include/margen.h).

## Conventions

- **Line endings:** Unix (LF). See [`.gitattributes`](.gitattributes) and [`.editorconfig`](.editorconfig).
- GDExtension sources remain C++ (godot-cpp); consume margen only through the **C ABI**.
- Algorithm style lives in margen’s [docs/rust-style.md](../margen/docs/rust-style.md).
