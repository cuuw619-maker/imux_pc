from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

required = [
    "native/CMakeLists.txt",
    "native/imux_launcher.manifest",
    "native/cpp/ImuxLauncher.cpp",
    "native/cpp/imux_launcher_functions.cpp",
    "native/cpp/imux_launcher_functions.h",
    "native/cpp/imux_3d_engine.cpp",
    "native/include/imux_3d_engine.h",
    "native/shaders/imux_ui.hlsl",
    "native/shaders/imux_world.hlsl",
    "native/shaders/imux_ui.glsl",
    "native/shaders/imux_ui.wgsl",
    "native/shaders/imux_world.glsl",
    "native/shaders/imux_world.wgsl",
    "rust/Cargo.toml",
    "rust/crates/imux-core/src/world.rs",
    "launcher/src/main/kotlin/com/imux/launcher/LauncherMetadataStore.kt",
    "launcher/src/main/resources/icon.webp",
    "tools/python/convert_icon.py",
    "core/src/main/kotlin/com/imux/core/release/ReleaseNotes.kt",
    "runtime/src/main/kotlin/com/imux/runtime/GameLaunchServiceImpl.kt",
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

launcher = (ROOT / "native/cpp/ImuxLauncher.cpp").read_text(encoding="utf-8")
functions = (ROOT / "native/cpp/imux_launcher_functions.cpp").read_text(encoding="utf-8")
release = (ROOT / "core/src/main/kotlin/com/imux/core/release/ReleaseNotes.kt").read_text(encoding="utf-8")

checks = {
    "responsive 1920 canvas": "kDesignWidth = 1920.0f" in launcher and "kDesignHeight = 1080.0f" in launcher,
    "scaled centered viewport": "CalculateUiViewport" in launcher and "kDesignScale = 0.82f" in launcher,
    "native transform hit-test": "ClientToUiPoint(float x, float y)" in launcher and "(x - viewport.offsetX) / viewport.scale" in launcher,
    "input shares render layout": "CalculateLauncherLayout" in launcher and "ClientToUiPoint" in launcher,
    "real game launch boundary": "CreateProcessW" in functions and "ImuxGame.exe" in functions and "imux_launcher_try_launch_game" in launcher,
    "built-in world fallback": "imux_world_run" in launcher,
    "F11 fullscreen": "VK_F11" in launcher and "ToggleFullscreen" in launcher,
    "DPI manifest": "PerMonitorV2" in (ROOT / "native/imux_launcher.manifest").read_text(encoding="utf-8"),
    "world movement": "GetAsyncKeyState('W')" in (ROOT / "native/cpp/imux_3d_engine.cpp").read_text(encoding="utf-8") and "GetCursorPos" in (ROOT / "native/cpp/imux_3d_engine.cpp").read_text(encoding="utf-8"),
    "world rendering": "D3D11CreateDeviceAndSwapChain" in (ROOT / "native/cpp/imux_3d_engine.cpp").read_text(encoding="utf-8"),
    "release version": 'CURRENT_VERSION = "0.0.1"' in release,
    "native version": 'kVersion[] = L"0.0.1"' in launcher,
    "changelog page": "Changelog" in (ROOT / "launcher/src/main/kotlin/com/imux/launcher/LauncherApp.kt").read_text(encoding="utf-8"),
    "version lock": "do not increment the version until the current release is approved" in (ROOT / "CHANGELOG.md").read_text(encoding="utf-8").lower(),
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    raise SystemExit("Project validation failed: " + ", ".join(failed))

print("Imux launcher, engine boundaries and release metadata: OK")
