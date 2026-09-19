package com.imux.core.model
import kotlinx.serialization.Serializable

@Serializable
data class RuntimeConfig(
    val javaPath: String? = null,
    val minMemoryMb: Int = 1024,
    val maxMemoryMb: Int = 4096,
    val extraArguments: List<String> = emptyList()
)
@Serializable
data class InstallationProfile(
    val id: String,
    val name: String,
    val version: String,
    val gameDir: String,
    val javaPath: String? = null,
    val memoryMinMb: Int = 1024,
    val memoryMaxMb: Int = 4096,
    val arguments: List<String> = emptyList(),
    val enabledMods: List<String> = emptyList(),
    val status: InstallationStatus = InstallationStatus.NOT_INSTALLED
)
@Serializable
enum class InstallationStatus { NOT_INSTALLED, READY, RUNNING, ERROR }
@Serializable
data class LauncherSettings(
    val startMinimized: Boolean = false,
    val closeLauncherOnGameStart: Boolean = false,
    val checkForUpdatesOnStart: Boolean = true
)
@Serializable
data class LauncherConfig(
    val schemaVersion: Int = 1,
    val selectedInstallationId: String = "default",
    val installations: List<InstallationProfile> = listOf(
        InstallationProfile("default", "Imux Development", "dev", "instances/default")
    ),
    val runtime: RuntimeConfig = RuntimeConfig(),
    val settings: LauncherSettings = LauncherSettings()
)
