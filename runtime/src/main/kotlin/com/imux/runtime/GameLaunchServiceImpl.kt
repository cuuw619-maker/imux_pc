package com.imux.runtime

import com.imux.core.model.InstallationProfile
import com.imux.core.service.GameLaunchService
import com.imux.core.service.LaunchResult
import com.imux.core.service.ProcessService
import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.Paths
import kotlin.io.path.Path

class GameLaunchServiceImpl(
    private val processService: ProcessService = ProcessServiceImpl()
) : GameLaunchService {

    override suspend fun launch(profile: InstallationProfile): LaunchResult {
        val instanceDir = Path(profile.gameDir).let { path ->
            if (path.isAbsolute) path else ImuxPaths.root.resolve(path)
        }

        val candidates = buildList {
            add(instanceDir.resolve("ImuxGame.exe"))
            add(instanceDir.resolve("game").resolve("ImuxGame.exe"))
            add(ImuxPaths.root.resolve("game").resolve("ImuxGame.exe"))
            add(currentDistributionRoot().resolve("ImuxGame.exe"))
        }.distinct()

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

    private fun currentDistributionRoot(): Path {
        val location = runCatching {
            Paths.get(
                GameLaunchServiceImpl::class.java.protectionDomain
                    .codeSource.location.toURI()
            ).toAbsolutePath().normalize()
        }.getOrNull() ?: return Paths.get(System.getProperty("user.dir")).toAbsolutePath()

        return when {
            Files.isRegularFile(location) -> location.parent?.parent ?: location.parent ?: location
            location.fileName.toString().equals("classes", ignoreCase = true) ->
                location.parent?.parent?.parent ?: location
            else -> location.parent ?: location
        }
    }
}
