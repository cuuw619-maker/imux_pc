from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

required = [
    "native/CMakeLists.txt",
    "native/cpp/ImuxLauncher.cpp",
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
world = (ROOT / "native/cpp/imux_3d_engine.cpp").read_text(encoding="utf-8")

checks = {
    "responsive centered layout": "contentW" in launcher and "1240.0f" in launcher,
    "play boundary": "PLAY" in launcher and "imux_world_run" in launcher,
    "world movement": "GetAsyncKeyState('W')" in world and "GetCursorPos" in world,
    "world rendering": "D3D11CreateDeviceAndSwapChain" in world,
    "world shader": "VSMain" in (ROOT / "native/shaders/imux_world.hlsl").read_text(encoding="utf-8"),
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    raise SystemExit("Project validation failed: " + ", ".join(failed))

print("Imux project structure and engine boundaries: OK")
