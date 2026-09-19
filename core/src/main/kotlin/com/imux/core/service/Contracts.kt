package com.imux.core.service
import com.imux.core.model.LauncherConfig
import com.imux.core.model.InstallationProfile
import com.imux.core.model.RuntimeConfig
import java.nio.file.Path

interface ConfigRepository { suspend fun load(): LauncherConfig; suspend fun save(config: LauncherConfig) }
interface RuntimeValidator { suspend fun validate(runtime: RuntimeConfig): RuntimeValidation }
data class RuntimeValidation(val available: Boolean, val executable: Path? = null, val message: String? = null)
interface GameLaunchService { suspend fun launch(profile: InstallationProfile): LaunchResult }
data class LaunchResult(val started: Boolean, val processId: Long? = null, val message: String)
interface ProcessService { suspend fun start(command: List<String>, workingDirectory: Path): LaunchResult }
