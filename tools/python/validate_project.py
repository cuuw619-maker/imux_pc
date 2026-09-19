from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

required = [
    "native/CMakeLists.txt",
    "native/imux_game.manifest",
    "native/imux_game.rc.in",
    "native/cpp/ImuxGame.cpp",
    "native/cpp/imux_3d_engine.cpp",
    "native/include/imux_3d_engine.h",
    "native/shaders/imux_world.hlsl",
    "native/shaders/imux_world.glsl",
    "native/shaders/imux_world.wgsl",
    "rust/Cargo.toml",
    "rust/crates/imux-core/src/world.rs",
    "launcher/src/main/kotlin/com/imux/launcher/LauncherApp.kt",
    "launcher/src/main/kotlin/com/imux/launcher/LauncherMetadataStore.kt",
    "launcher/src/main/resources/icon.webp",
    "runtime/src/main/kotlin/com/imux/runtime/GameLaunchServiceImpl.kt",
    "core/src/main/kotlin/com/imux/core/release/ReleaseNotes.kt",
    "CHANGELOG.md",
    "native/assets/blocks/dirt.png",
    "native/assets/blocks/stone.png",
    "native/assets/gui/crosshair.png",
    "native/assets/gui/hotbar.png",
    "settings.gradle.kts",
]

missing = [p for p in required if not (ROOT / p).exists()]
if missing:
    raise SystemExit("Missing project files: " + ", ".join(missing))

launcher = (ROOT / "launcher/src/main/kotlin/com/imux/launcher/LauncherApp.kt").read_text(encoding="utf-8")
gradle = (ROOT / "launcher/build.gradle.kts").read_text(encoding="utf-8")
runtime = (ROOT / "runtime/src/main/kotlin/com/imux/runtime/GameLaunchServiceImpl.kt").read_text(encoding="utf-8")
cmake = (ROOT / "native/CMakeLists.txt").read_text(encoding="utf-8")
game = (ROOT / "native/cpp/ImuxGame.cpp").read_text(encoding="utf-8")
world = (ROOT / "native/cpp/imux_3d_engine.cpp").read_text(encoding="utf-8")
release = (ROOT / "core/src/main/kotlin/com/imux/core/release/ReleaseNotes.kt").read_text(encoding="utf-8")

checks = {
    "kotlin compose launcher": "fun LauncherApp()" in launcher and "Button(" in launcher and "AnimatedBackdrop" in launcher,
    "minimal launcher ui": "Text("ИГРАТЬ"" in launcher and "SettingsPage" not in launcher and "enum class Page" not in launcher,
    "compose windows exe": "TargetFormat.Exe" in gradle and "createDistributable" in (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8"),
    "dedicated game target": "add_executable(ImuxGame WIN32" in cmake and "cpp/ImuxGame.cpp" in cmake and "cpp/ImuxLauncher.cpp" not in cmake,
    "game launch path": "ImuxGame.exe" in runtime,
    "d3d feature level argument": "creationFlags, levels, 1, D3D11_SDK_VERSION" in world,
    "world runtime loop": "imux_world_update(dt)" in game and "imux_world_render()" in game,
    "world rendering": "D3D11CreateDeviceAndSwapChain" in world,
    "world movement": "GetAsyncKeyState('W')" in world and "GetCursorPos" in world,
    "release version": 'CURRENT_VERSION = "0.0.1"' in release,
    "no native launcher target": "ImuxLauncher" not in cmake,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    raise SystemExit("Project validation failed: " + ", ".join(failed))

print("Kotlin launcher, dedicated game runtime and release metadata: OK")
