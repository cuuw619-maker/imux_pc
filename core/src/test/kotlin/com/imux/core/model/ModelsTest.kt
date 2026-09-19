package com.imux.core.model
import kotlinx.serialization.json.Json
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
class ModelsTest {
    private val json = Json { ignoreUnknownKeys = true }
    @Test fun configurationRoundTrips() {
        val source = LauncherConfig(installations = listOf(
            InstallationProfile("dev", "Development", "dev", "instances/dev")
        ), selectedInstallationId = "dev")
        val decoded = json.decodeFromString<LauncherConfig>(json.encodeToString(source))
        assertEquals(source, decoded)
    }
    @Test fun defaultProfileIsPresent() { assertTrue(LauncherConfig().installations.any { it.id == "default" }) }
}
