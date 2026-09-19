# Imux

Imux is an independent Windows-first Minecraft-like client project. The project is developed from original code and architecture and does not contain or depend on Minecraft source code or a proprietary Minecraft implementation.

## Status

Phase 1: Launcher foundation.

Implemented in this phase:
- Kotlin/JVM + Compose Multiplatform Desktop launcher
- framework-independent core/domain module
- runtime and process abstractions
- Windows user-data path management
- JSON configuration persistence
- mock launch flow for validating the launcher lifecycle
- C/C++/CMake native bootstrap
- Rust workspace foundation
- future Lua/native modding API boundaries
- unit tests
- GitHub Actions CI and Windows distribution packaging

The actual 3D game engine is intentionally not implemented yet.

## Architecture

```
launcher (Compose UI)
    -> core (domain/application contracts)
    -> runtime (filesystem, process, launch services)
    -> future client runtime / native engine

native/
    C/C++ bootstrap library, isolated from launcher UI

rust/
    Rust workspace for future low-level systems and tooling

modding/
    versioned API contracts for future Lua/native extensions

tools/python/
    developer-only tooling; never a launcher runtime dependency
```

The launcher UI does not own filesystem, process, configuration, or runtime logic. UI state is driven through application services and state holders.

## Technology

- Kotlin / Java JVM
- Compose Multiplatform Desktop
- Gradle Kotlin DSL
- Java 21 toolchain
- Windows x64 first
- C / C++ / CMake
- Rust
- Python for future development tooling

## Development

Requirements:
- JDK 21+
- Gradle 8.10+ or the Gradle wrapper when available
- CMake 3.20+
- a C/C++ compiler for the native module
- Rust stable for the Rust module

Run the launcher:

```text
gradle :launcher:run
```

Run tests:

```text
gradle test
```

Build the Windows application image:

```text
gradle :launcher:packageReleaseDistributionForCurrentOS
```

The CI workflow builds the Windows package on `windows-latest` and publishes it as an artifact.

## User data

Imux uses the Windows application-data location rather than the current working directory:

```text
%APPDATA%\\Imux
```

The layout is separated into configuration, instances, logs, runtime files, downloads, and future assets.

## Native layer

`native/` contains a deliberately small C/C++ bootstrap library. The launcher does not require this library to start. A future client runtime can load or communicate with native code through a dedicated bridge without coupling Compose UI to the engine.

## Rust layer

`rust/` is an independent workspace. It is reserved for low-level systems, asset processing, networking, and other performance-sensitive components. Rust is not a runtime dependency of the launcher at this stage.

## Modding

`modding/` defines stable conceptual boundaries for:
- ModLoader
- ModMetadata
- ModContext
- ModAPI
- ModRuntime

Lua will be supported by a future runtime layer. Native extensions may eventually be supported through a controlled ABI/API. Mods are not granted unrestricted launcher filesystem access by this architecture.

## Repository layout

```
core/
launcher/
runtime/
modding/
native/
rust/
tools/python/
.github/workflows/
```

## Roadmap

1. Launcher foundation
2. Installation/runtime management
3. Native engine bootstrap
4. C++/C/Rust engine systems
5. Window/input
6. Rendering
7. Voxel/block world
8. Chunks and world systems
9. Entities
10. Networking
11. Assets/resources
12. Lua and native modding
13. Advanced launcher
14. Installer/updater/distribution

Imux is intentionally developed in this order so the launcher remains independent from the future game engine.
