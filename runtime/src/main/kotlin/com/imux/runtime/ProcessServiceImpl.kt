package com.imux.runtime

import com.imux.core.service.LaunchResult
import com.imux.core.service.ProcessService
import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.StandardOpenOption
import kotlin.concurrent.thread
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class ProcessServiceImpl : ProcessService {
    override suspend fun start(command: List<String>, workingDirectory: Path): LaunchResult =
        withContext(Dispatchers.IO) {
            try {
                Files.createDirectories(workingDirectory)
                val process = ProcessBuilder(command)
                    .directory(workingDirectory.toFile())
                    .redirectErrorStream(true)
                    .start()

                val logFile = ImuxPaths.logs.resolve("launch-" + System.currentTimeMillis() + ".log")
                thread(name = "imux-process-log", isDaemon = true) {
                    runCatching {
                        Files.newBufferedWriter(
                            logFile,
                            Charsets.UTF_8,
                            StandardOpenOption.CREATE,
                            StandardOpenOption.TRUNCATE_EXISTING,
                            StandardOpenOption.WRITE
                        ).use { writer ->
                            process.inputStream.bufferedReader().useLines { lines ->
                                lines.forEach {
                                    writer.appendLine(it)
                                    writer.flush()
                                }
                            }
                            writer.appendLine("Process exited with code " + process.waitFor() + ".")
                        }
                    }
                }

                LaunchResult(
                    started = true,
                    processId = process.pid(),
                    message = "Process started."
                )
            } catch (e: Exception) {
                LaunchResult(
                    started = false,
                    message = e.message ?: "Unable to start process."
                )
            }
        }
}
