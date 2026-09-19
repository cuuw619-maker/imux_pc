package com.imux.core.release

data class ReleaseNote(
    val version: String,
    val date: String,
    val entries: List<String>
)

object ImuxRelease {
    const val CURRENT_VERSION = "0.0.1"

    val history: List<ReleaseNote> = listOf(
        ReleaseNote(
            version = CURRENT_VERSION,
            date = "2026-09-19",
            entries = listOf(
                "Launcher rebuilt around a new responsive launch workspace.",
                "Play now uses a real game launch boundary instead of a mock process.",
                "Built-in first-person world remains available as the base game target.",
                "Changelog and release version are visible directly in the launcher."
            )
        )
    )
}
