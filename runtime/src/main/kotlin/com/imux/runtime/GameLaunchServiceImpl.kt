package com.imux.runtime

import com.imux.core.model.InstallationProfile
import com.imux.core.service.GameLaunchService
import com.imux.core.service.LaunchResult
import com.imux.core.service.ProcessService
import java.nio.file.Files
import kotlin.io.path.Path

class GameLaunchServiceImpl(
    private val processService: ProcessService = ProcessServiceImpl()
) : GameLaunchService {

    override suspend fun launch(profile: InstallationProfile): LaunchResult {
        val instanceDir = Path(profile.gameDir).let { path ->
            if (path.isAbsolute) path else ImuxPaths.root.resolve(path)
        }

        val candidates = listOf(
            instanceDir.resolve("ImuxGame.exe"),
            instanceDir.resolve("game").resolve("ImuxGame.exe"),
            ImuxPaths.root.resolve("game").resolve("ImuxGame.exe")
        ).distinct()

        val executable = candidates.firstOrNull { Files.isRegularFile(it) }
            ?: return LaunchResult(
                started = false,
                message = "Game executable is not installed yet."
            )

        return processService.start(
            command = buildList {
                add(executable.toString())
                addAll(profile.arguments)
            },
            workingDirectory = executable.parent
        )
    }
}
