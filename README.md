# margen-godot

Godot **GDExtension** that integrates the [margen](../margen) world-generation library into Godot 4.x.

Generation algorithms stay in **margen**. This repository only:

- Loads/links margen
- Converts margen datasets into Godot meshes, nodes, and materials
- Exposes a thin API to GDScript / C# games (e.g. Marloth)

## Status

Scaffold / placeholder. Extension binding code is not implemented yet.

## Layout (planned)

| Path | Purpose |
|------|---------|
| `src/` | GDExtension C++ sources |
| `doc_classes/` / `.gdextension` | Godot registration (when added) |
| `.devcontainer/` | C++ / future godot-cpp toolchain |

See [AGENTS.md](AGENTS.md).
