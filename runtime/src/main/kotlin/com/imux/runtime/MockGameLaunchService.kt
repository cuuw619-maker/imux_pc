package com.imux.runtime
import com.imux.core.model.InstallationProfile
import com.imux.core.service.GameLaunchService
import com.imux.core.service.LaunchResult
import com.imux.core.service.ProcessService
class MockGameLaunchService(private val processService: ProcessService = ProcessServiceImpl()) : GameLaunchService {
    override suspend fun launch(profile: InstallationProfile): LaunchResult {
        val workDir = ImuxPaths.instances.resolve(profile.id)
        val message = "Imux development runtime started for " + profile.name
        val command = if (System.getProperty("os.name").startsWith("Windows", true))
            listOf("cmd.exe", "/c", "echo " + message + " & timeout /t 2 /nobreak >nul")
        else listOf("sh", "-c", "echo '" + message + "'; sleep 2")
        return processService.start(command, workDir)
    }
}