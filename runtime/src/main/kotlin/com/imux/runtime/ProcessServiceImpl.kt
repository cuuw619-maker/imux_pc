package com.imux.runtime
import com.imux.core.service.LaunchResult
import com.imux.core.service.ProcessService
import java.nio.file.Files
import java.nio.file.Path
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
class ProcessServiceImpl : ProcessService {
    override suspend fun start(command: List<String>, workingDirectory: Path): LaunchResult = withContext(Dispatchers.IO) {
        try {
            Files.createDirectories(workingDirectory)
            val process = ProcessBuilder(command).directory(workingDirectory.toFile()).redirectErrorStream(true).start()
            val logFile = ImuxPaths.logs.resolve("launch-" + System.currentTimeMillis() + ".log")
            logFile.toFile().bufferedWriter().use { writer ->
                process.inputStream.bufferedReader().useLines { lines -> lines.forEach { writer.appendLine(it); writer.flush() } }
            }
            LaunchResult(true, process.pid(), "Process exited with code " + process.waitFor() + ".")
        } catch (e: Exception) { LaunchResult(false, message = e.message ?: "Unable to start process.") }
    }
}