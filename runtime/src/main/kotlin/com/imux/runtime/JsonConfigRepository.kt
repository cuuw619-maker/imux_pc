package com.imux.runtime
import com.imux.core.model.LauncherConfig
import com.imux.core.service.ConfigRepository
import kotlinx.serialization.json.Json
import kotlin.io.path.createDirectories
import kotlin.io.path.exists
import kotlin.io.path.readText
import kotlin.io.path.writeText
class JsonConfigRepository : ConfigRepository {
    private val json = Json { prettyPrint = true; ignoreUnknownKeys = true }
    override suspend fun load(): LauncherConfig {
        ImuxPaths.initialize()
        if (!ImuxPaths.config.exists()) return LauncherConfig().also { save(it) }
        return json.decodeFromString(ImuxPaths.config.readText())
    }
    override suspend fun save(config: LauncherConfig) {
        ImuxPaths.config.parent.createDirectories()
        ImuxPaths.config.writeText(json.encodeToString(config))
    }
}