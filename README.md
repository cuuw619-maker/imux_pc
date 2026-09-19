# Imux

Imux is an independent Windows-first Minecraft-like client project built from original code and architecture. It does not contain or depend on Minecraft source code or a proprietary Minecraft implementation.

## Current direction

The launcher is being split into two deliberate layers:

- **Native Imux UI Engine** — C++/Win32 + Direct2D + DirectWrite, with a Direct3D 11/HLSL rendering pipeline foundation.
- **Launcher services** — Kotlin/JVM modules for configuration, runtime validation, process management and application/domain contracts.

Compose remains a first-class hybrid UI shell with Material 3 / Material You design, while the native C++ renderer provides the Windows rendering foundation. The two layers share the same launcher-service boundary; neither owns the other's business logic.

The native UI is an original Imux design. XMCL/X-Minecraft Launcher is used only as a reference for general launcher UX patterns such as instance-oriented organization, renderer/runtime separation, explicit launch state, and structured settings.

## UI engine

The current native prototype provides:

- adaptive Windows desktop layout for windowed/maximized/fullscreen states
- DPI-aware responsive sizing without a persistent sidebar
- animated hamburger navigation drawer
- shared visual/hit-test geometry so buttons respond where they are drawn
- profile/instance card
- primary Play action with hover and launch state
- runtime status area
- technology/engine stack area
- DPI-aware windowing
- minimum window size
- Direct2D/DirectWrite text and shape rendering
- Direct3D 11 device initialization
- HLSL pixel-shader compilation path

The native source lives in `native/cpp/` and the shader layer in `native/shaders/`.

The renderer is deliberately being built as an Imux-owned layer instead of treating a stock widget toolkit as the final visual architecture. Over time this layer will gain its own layout primitives, animation system, input routing, render graph, resource cache and GPU effects.

## Technology stack

| Layer | Technology | Purpose |
| --- | --- | --- |
| UI engine | C++ / Win32 | native Windows windowing and UI runtime |
| 2D rendering | Direct2D / DirectWrite | text, panels, primitives |
| GPU pipeline | Direct3D 11 / HLSL | shader and future accelerated effects |
| Systems | C / C++ | ABI and native bootstrap |
| Application services | Kotlin / JVM | configuration, process and runtime services |
| Low-level systems | Rust | future engine, networking, assets and performance-sensitive systems |
| Build | CMake / Gradle | native and JVM build graphs |
| Future UI/tooling | C# / XAML | optional Windows tooling/prototypes where it provides a concrete advantage |
| Developer tools | Python | asset processing, validation and generation only |
| Cross-platform shader foundation | GLSL / WGSL | future OpenGL/WebGPU backends |
| Local metadata | SQLite | launcher metadata and future cache/index state |
| Windows tooling | C# / WinUI 3 / XAML | optional native Windows prototypes and integration tooling |

The languages are not mixed arbitrarily. Each layer has a defined boundary and ABI/API.

## Architecture

~~~text
Native Windows UI
    C++ / Win32
        |
        +-- Imux UI Engine
        |     +-- layout
        |     +-- input
        |     +-- animation
        |     +-- Direct2D / DirectWrite
        |     +-- D3D11 / HLSL
        |
        +-- launcher IPC/API boundary
                  |
                  v
        Kotlin Launcher Services
        +-- core
        |    +-- models
        |    +-- UI/application state
        |    +-- service contracts
        |
        +-- runtime
             +-- configuration
             +-- filesystem
             +-- Java/runtime validation
             +-- process lifecycle

Future:
        native engine
        C / C++ / Rust
             |
             +-- renderer
             +-- world/chunks
             +-- networking
             +-- assets
             +-- modding ABI
~~~

The important rule is that the UI does not own launcher business logic. Native rendering and application services communicate through explicit contracts.

## Repository layout

~~~text
core/                 Kotlin domain and service contracts
runtime/              Kotlin filesystem/process/runtime services
launcher/             Compose development shell
modding/              future mod API boundaries

native/
  cpp/                native launcher and UI engine
  c/                  C ABI bootstrap
  include/            native public headers
  shaders/            HLSL shaders
  CMakeLists.txt      native build graph

rust/
  crates/imux-core/   Rust low-level foundation

tools/python/         developer-only Python tools
.github/workflows/    CI
CMakeLists.txt        root native build entrypoint
~~~

## Windows build

The production launcher build is native and does **not** create an MSI/installer.

Requirements:

- Windows 10/11 x64
- Visual Studio Build Tools or Visual Studio with C++ desktop workload
- CMake 3.20+
- JDK 21+ for JVM tests/services
- Rust stable for the Rust workspace

Build the native launcher:

~~~text
cmake -S . -B build/native -A x64
cmake --build build/native --config Release
~~~

The executable is produced at:

~~~text
build/native/bin/ImuxLauncher.exe
~~~

The HLSL source is copied beside the executable for development inspection.

Run JVM tests:

~~~text
gradle test
~~~

Build all JVM modules:

~~~text
gradle build
~~~

## User data

Imux uses the Windows application-data directory instead of the current working directory:

~~~text
%APPDATA%\Imux
~~~

The runtime separates configuration, instances, logs, runtime files, downloads and assets.

## Runtime and launcher logic

The launcher service layer remains independent from the visual engine. The intended launch flow is:

~~~text
resolve instance
 -> validate runtime
 -> resolve libraries/assets
 -> construct command
 -> start process
 -> capture output
 -> track exit state
~~~

The current development profile still uses a mock runtime. Real client/runtime integration is a later phase.

## UI design direction

The launcher UI is intentionally product-oriented rather than technology-oriented. Home, Instances and Engine no longer expose the implementation stack as permanent dashboard content. Technology details are grouped under Settings > Advanced. The primary Play action is anchored to the bottom-right action area, and responsive layout calculations are based on the current client bounds rather than fixed desktop coordinates.

The Compose shell uses Material 3 semantics, adaptive sizing, modal navigation and motion. The native prototype mirrors the same interaction model with a custom Imux drawer and explicit hit-testing geometry.

## Native engine direction

The native UI is the first concrete piece of the custom engine direction. It is intentionally small now.

The planned engine layers are:

1. platform/window abstraction
2. input/event system
3. resource and asset cache
4. layout and UI primitives
5. animation/timing system
6. render graph
7. Direct3D backend
8. HLSL shader library
9. math primitives and transforms
10. future game renderer

The mathematical and rendering code will be developed as Imux-owned systems rather than copied from another launcher or game.

## Modding

`modding/` defines API boundaries for future extensions. Lua is planned for higher-level scripting, while native extensions may use a controlled C ABI and later Rust/C++ bindings.

## CI

GitHub Actions validates:

- Kotlin tests and JVM module builds
- native C/C++/CMake build on Windows
- Rust workspace
- native `ImuxLauncher.exe` production

The Windows CI artifact is the launcher executable, not an installer.

## Roadmap

1. Native launcher UI engine
2. Launcher/runtime IPC boundary
3. Real instance management
4. Runtime and asset management
5. Native engine bootstrap
6. C++/C/Rust engine systems
7. Input and window subsystem
8. Rendering backend and HLSL library
9. Voxel/block world
10. Chunks and world systems
11. Entities and networking
12. Assets/resources
13. Lua/native modding
14. Advanced launcher features
15. Distribution/update system

The project is deliberately developed as an independent implementation rather than by copying Minecraft or another launcher's source code.
