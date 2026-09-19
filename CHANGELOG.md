# Changelog

## 0.0.1 — 2026-09-19

- Reset the project to a minimal Windows architecture.
- Launcher reduced to one Kotlin/JVM application with one main action: ИГРАТЬ.
- Removed Compose, runtime services, database, Rust and the old Windows UI from the active build.
- Kept the game as a separate ImuxGame.exe process.
- Replaced the native renderer bootstrap with a small self-contained Direct3D 11 runtime.
- Simplified GitHub Actions to one Windows build that produces the launcher, game and combined ZIP artifact.

Version remains 0.0.1.
