# Imux

Imux is an independent Windows-first Minecraft-like client project built from original code and architecture.

## Current launcher architecture

The Windows launcher is Kotlin + Jetpack Compose Desktop. The launcher UI is intentionally minimal at this stage: a single main action, ИГРАТЬ, with an animated background. Navigation, settings, changelog and other product surfaces are deliberately postponed until the base launch flow is stable.

The 3D runtime is a separate ImuxGame.exe. This keeps the launcher UI completely independent from the native renderer while allowing the world engine to evolve separately.

```text
Imux.exe
  Kotlin / JVM
  Jetpack Compose Desktop
      |
      +-- launcher state
      +-- process launch service
      +-- future instance manager

ImuxGame.exe
  C++20
  Win32
  Direct3D 11 / HLSL
      |
      +-- window/input
      +-- first-person camera
      +-- voxel test scene
      +-- future world/chunk systems
```

## Technology stack

| Layer | Technology | Purpose |
| --- | --- | --- |
| Windows launcher | Kotlin / JVM | application entrypoint and launcher UI |
| UI | Jetpack Compose Desktop | Windows launcher presentation |
| Launcher services | Kotlin | configuration, process management and runtime contracts |
| Game runtime | C++20 | native game process and rendering runtime |
| Graphics | Direct3D 11 / HLSL | world renderer |
| Native platform | Win32 | game window and input |
| Low-level foundation | Rust | future engine and performance-sensitive systems |
| Build | Gradle / CMake | JVM and native build graphs |
| Tools | Python | asset conversion and validation |

## Repository layout

```text
core/                 Kotlin domain models and service contracts
runtime/              Kotlin filesystem/process/runtime services
launcher/             Kotlin Compose Windows launcher
modding/              future mod API boundaries

native/
  cpp/                ImuxGame runtime
  c/                  native C ABI foundation
  include/            native public headers
  shaders/            world shaders
  assets/             world assets
  CMakeLists.txt      native game build graph

rust/                 Rust low-level foundation
tools/python/         validation and asset tools
.github/workflows/    CI
```

## Windows development

Build the Kotlin launcher distribution:

```text
gradle :launcher:createDistributable
```

The executable is produced inside the Compose application image:

```text
build/compose/binaries/main/app/Imux/Imux.exe
```

Build the native game runtime:

```text
cmake -S . -B build/native -A x64
cmake --build build/native --config Release
```

The game runtime is produced at:

```text
build/native/bin/ImuxGame.exe
```

For a runnable local distribution, place ImuxGame.exe and the assets directory beside Imux.exe inside the packaged Imux application directory.

## Launch flow

The launcher resolves a development profile, locates ImuxGame.exe and starts it as a separate process. The launcher does not render the 3D world itself.

The current base world contains a small test block scene, first-person mouse look, WASD movement, sprint, jump, camera smoothing and basic atmospheric shading. It is an engine bootstrap, not a finished game world.

## CI

GitHub Actions validates Kotlin, Rust, the native game runtime and the packaged Windows Compose launcher. The Windows artifact contains both Imux.exe and ImuxGame.exe.

## Release policy

The project remains on version 0.0.1. The version is not incremented until the current release is explicitly approved.
