from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
required = [
    "native/CMakeLists.txt",
    "native/cpp/ImuxLauncher.cpp",
    "native/shaders/imux_ui.hlsl",
    "native/shaders/imux_ui.glsl",
    "native/shaders/imux_ui.wgsl",
    "rust/Cargo.toml",
    "settings.gradle.kts",
]

missing = [p for p in required if not (ROOT / p).exists()]
if missing:
    raise SystemExit("Missing project files: " + ", ".join(missing))

print("Imux project structure: OK")
