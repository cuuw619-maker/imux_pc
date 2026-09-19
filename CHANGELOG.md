# Changelog

## 0.0.1 — 2026-09-19

- Rebuilt the Windows launcher around Kotlin and Jetpack Compose Desktop.
- Reduced the launcher base screen to the single primary ИГРАТЬ action.
- Removed the old native C++ launcher UI from the production build.
- Split the game runtime into a dedicated ImuxGame.exe process.
- Fixed the built-in D3D11 startup path by correcting the feature-level array count passed to D3D11CreateDeviceAndSwapChain.
- Added fallback startup paths and runtime diagnostics for the native game renderer.
- Kept the release at 0.0.1 until the project owner explicitly approves the next version.

Versioning rule: do not increment the version until the current release is approved. Each approved release appends only its actual changes to this file and to the launcher metadata.
