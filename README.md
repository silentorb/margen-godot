# margen-godot

Godot **GDExtension** that integrates the [margen](../margen) world-generation library into Godot 4.x via margen’s **C ABI**.

Generation algorithms stay in **margen** (Rust). This repository:

- Loads/links `margen-ffi` (`include/margen.h`)
- Converts margen face datasets into Godot meshes and placeholder materials
- Exposes `MargenWorldMesh` to GDScript / C# games (e.g. Marloth)

## Status

Minimal mesh path: `MargenWorldMesh` calls `margen_generate_world_faces` and builds an `ArrayMesh`.

Face points from the ABI keep margen’s RH-outward (CCW) order; `MargenWorldMesh` reverses triangle fans for Godot’s clockwise front faces (see margen `docs/coordinates.md`).

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

## Build (Windows natives)

Cross-compile via Marloth’s **`marloth-win`** image (cargo-xwin + clang-cl + pre-cached xwin CRT/SDK). From the attached Marloth session:

```bash
# Prefer: triggers marloth-win HTTP agent on the compose network
./scripts/devcontainer.sh windows-build
```

Or inside `marloth-win` / with the toolchain on PATH:

```bash
TARGET=windows ./scripts/build.sh
```

Requires `XWIN_CACHE_DIR` (default `/opt/cargo-xwin`) from the **marloth-win** image — do not download the CRT/SDK at build time. Rebuild `marloth-win` if the cache is missing.

## Install into Marloth

After building:

```bash
./scripts/install-to-marloth.sh
```

This copies built libraries into marloth’s `addons/margen/bin/` (the checked-in `.gdextension` already has Marloth-relative paths).

## Layout

| Path | Purpose |
|------|---------|
| `src/` | GDExtension C++ sources |
| `godot-cpp/` | git submodule (branch 4.5) |
| `margen_godot.gdextension` | Godot registration |
| `bin/` | built shared libraries (local) |
| `scripts/` | build + install + `verify-godot-cpp-api.sh` |

### Diagnostics

- Init logs to stderr / Godot warnings: `margen_godot: library_init entered`, `library_init returned`, `initialized (MargenWorldMesh registered, api=...)`.
- API alignment audit (no Godot required): `./scripts/verify-godot-cpp-api.sh 4.6.1`

See [AGENTS.md](AGENTS.md).
