# margen-godot

Godot **GDExtension** that integrates the [margen](../margen) world-generation library into Godot 4.x via margen’s **C ABI**.

Generation algorithms stay in **margen** (Rust). This repository:

- Loads/links `margen-ffi` (`include/margen.h`)
- Converts margen face datasets into Godot meshes and placeholder materials
- Exposes `MargenWorldMesh` to GDScript / C# games (e.g. Marloth)

## Status

Minimal mesh path: `MargenWorldMesh` calls `margen_generate_world_faces` and builds an `ArrayMesh`.

## Dev container

This repo’s [`.devcontainer/`](.devcontainer/) image holds the **focused** third-party stack other Marloth/margen containers omit:

- C++17 (`build-essential`, CMake, Ninja)
- Python 3 (godot-cpp binding generation)
- Rust 1.88 (build sibling **`margen`** `margen_ffi`)

**Reopen in Container** on the **margen-godot** folder (not the marloth workspace root). The container bind-mounts `../margen` at `/workspaces/margen` and sets `MARGEN_ROOT`. Rebuild the image after Dockerfile changes.

## Prerequisites (host / manual)

- Rust toolchain (build `margen_ffi`)
- CMake 3.17+, Ninja or Make, C++17 compiler
- Python 3 (godot-cpp binding generation)
- godot-cpp **4.5** submodule (compatible with Godot **4.6** editor)
- Sibling **`margen`** checkout (default `../margen`)

Initialize submodules:

```bash
git submodule update --init --recursive
```

## Build (Linux / WSL / dev container)

```bash
./scripts/build.sh
```

Outputs land in `bin/` (for example `libmargen_godot.linux.template_debug.x86_64.so`).

Environment overrides:

- `MARGEN_ROOT` — path to margen repo (default: sibling `../margen`)
- `BUILD_TYPE` — `Debug` or `Release`

## Install into Marloth

After building:

```bash
./scripts/install-to-marloth.sh
```

This copies the `.gdextension` file and built library into marloth’s `addons/margen/`.

## Layout

| Path | Purpose |
|------|---------|
| `src/` | GDExtension C++ sources |
| `godot-cpp/` | git submodule (branch 4.5) |
| `margen_godot.gdextension` | Godot registration |
| `bin/` | built shared libraries (local) |
| `scripts/` | build + install helpers |

See [AGENTS.md](AGENTS.md).
