package com.imux.runtime
import java.nio.file.Path
import kotlin.io.path.Path
import kotlin.io.path.createDirectories
object ImuxPaths {
    val root: Path = Path(System.getenv("APPDATA") ?: System.getProperty("user.home"), "Imux")
    val config = root.resolve("config/launcher.json")
    val instances = root.resolve("instances")
    val logs = root.resolve("logs")
    val runtime = root.resolve("runtime")
    val downloads = root.resolve("downloads")
    val assets = root.resolve("assets")
    fun initialize() { listOf(root, config.parent, instances, logs, runtime, downloads, assets).forEach { it.createDirectories() } }
}