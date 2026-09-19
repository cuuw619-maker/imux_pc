# Imux

Windows-first Minecraft-like project built from a minimal two-part architecture.

## Architecture

```
Imux.exe
  Kotlin / JVM / Swing
  |
  +-- one window
  +-- one ИГРАТЬ button
  +-- starts ImuxGame.exe

ImuxGame.exe
  C++20 / Win32 / Direct3D 11
  |
  +-- game window
  +-- WASD + mouse
  +-- jump
  +-- small voxel test scene
```

There are no launcher service layers, database, Rust dependency, Compose dependency, signing step or custom validation framework in the build path.

## Repository

```
launcher/       Kotlin launcher
native/         C++ game runtime
.github/        Windows CI
```

The native asset folder is kept for future world content, but the current renderer does not depend on external assets.

## Local build

Launcher:

```
gradle :launcher:installDist
```

Game:

```
cmake -S native -B build/native -A x64
cmake --build build/native --config Release
```

Game executable:

```
build/native/bin/ImuxGame.exe
```

For a local runnable copy, place `ImuxGame.exe` beside the launcher executable.

## Windows artifact

GitHub Actions builds both programs and packages them together as:

```
imux-windows-x64.zip
  Imux/
    Imux.exe
    ImuxGame.exe
    lib/
    runtime/
```

The launcher searches for `ImuxGame.exe` beside itself, from the current directory, and from the local development build directory.

## Current scope

The launcher deliberately contains only the main screen and the ИГРАТЬ action. Features will be added one at a time after the base build and launch path remain stable.

The game runtime is an intentionally small Direct3D 11 bootstrap. It is not yet a complete Minecraft implementation.

Version: 0.0.1
