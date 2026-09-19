package com.imux.launcher
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.imux.core.service.*
import com.imux.core.state.LauncherUiState
import com.imux.runtime.*
import kotlinx.coroutines.launch

@Composable
fun LauncherApp() {
    MaterialTheme {
        val repository: ConfigRepository = remember { JsonConfigRepository() }
        val validator: RuntimeValidator = remember { RuntimeValidatorImpl() }
        val gameLauncher: GameLaunchService = remember { MockGameLaunchService() }
        val scope = rememberCoroutineScope()
        var state by remember { mutableStateOf<LauncherUiState>(LauncherUiState.Loading) }
        var settings by remember { mutableStateOf(false) }

        LaunchedEffect(Unit) {
            runCatching {
                val config = repository.load()
                val profile = config.installations.firstOrNull { it.id == config.selectedInstallationId }
                    ?: config.installations.first()
                val runtime = validator.validate(config.runtime)
                LauncherUiState.Ready(profile, runtime.available)
            }.onSuccess { state = it }
             .onFailure { state = LauncherUiState.Error(it.message ?: "Unable to load launcher configuration.") }
        }

        Surface(Modifier.fillMaxSize()) {
            Row(Modifier.fillMaxSize()) {
                SideBar(settings, { settings = false }, { settings = true })
                if (settings) SettingsScreen() else HomeScreen(state) {
                    val ready = state as? LauncherUiState.Ready ?: return@HomeScreen
                    state = LauncherUiState.Launching(ready.profile)
                    scope.launch {
                        val result = gameLauncher.launch(ready.profile)
                        state = if (result.started)
                            LauncherUiState.Running(ready.profile, result.processId)
                        else LauncherUiState.Error(result.message)
                    }
                }
            }
        }
    }
}

@Composable
private fun SideBar(settings: Boolean, onHome: () -> Unit, onSettings: () -> Unit) {
    Column(
        Modifier.fillMaxHeight().width(190.dp).padding(18.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Text("IMUX", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(12.dp))
        TextButton(onClick = onHome) { Text("Home") }
        TextButton(onClick = onSettings) { Text("Settings") }
        Spacer(Modifier.weight(1f))
        Text("Windows x64", style = MaterialTheme.typography.labelSmall)
        if (settings) Text("Configuration", style = MaterialTheme.typography.labelSmall)
    }
}

@Composable
private fun HomeScreen(state: LauncherUiState, onPlay: () -> Unit) {
    val profile = when (state) {
        is LauncherUiState.Ready -> state.profile
        is LauncherUiState.Launching -> state.profile
        is LauncherUiState.Running -> state.profile
        else -> null
    }
    Box(Modifier.fillMaxSize().background(Color(0xFF101114)).padding(40.dp)) {
        Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.Center) {
            Text("IMUX", style = MaterialTheme.typography.displayLarge, color = Color.White)
            Text("Independent game client launcher", color = Color(0xFFB8BBC3))
            Spacer(Modifier.height(42.dp))
            Surface(Modifier.fillMaxWidth(), shape = RoundedCornerShape(18.dp), color = Color(0xFF191B20)) {
                Row(Modifier.fillMaxWidth().padding(24.dp), verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text(profile?.name ?: "Loading…", color = Color.White, style = MaterialTheme.typography.titleLarge)
                        Text("Profile: " + (profile?.version ?: "—"), color = Color(0xFF9EA2AC))
                        val status = when (state) {
                            is LauncherUiState.Ready -> if (state.runtimeAvailable) "Runtime ready" else "Runtime unavailable"
                            is LauncherUiState.Launching -> "Starting development runtime…"
                            is LauncherUiState.Running -> "Development runtime started (PID " + (state.pid ?: "unknown") + ")"
                            is LauncherUiState.Error -> state.message
                            LauncherUiState.Loading -> "Loading configuration…"
                        }
                        Text(status, color = Color(0xFF9EA2AC))
                    }
                    Button(onClick = onPlay, enabled = state is LauncherUiState.Ready) { Text("PLAY") }
                }
            }
        }
    }
}

@Composable
private fun SettingsScreen() {
    Column(Modifier.fillMaxSize().padding(42.dp), verticalArrangement = Arrangement.spacedBy(18.dp)) {
        Text("Settings", style = MaterialTheme.typography.headlineLarge)
        Text("Launcher configuration", style = MaterialTheme.typography.titleMedium)
        Text("Java / Runtime", style = MaterialTheme.typography.titleLarge)
        Text("The current JVM is used for development until a game runtime is configured.")
        Text("Game directory")
        Text("Stored under %APPDATA%\\Imux\\instances")
        Text("Memory")
        Text("Memory limits are persisted per InstallationProfile.")
        Text("Launcher behaviour")
        Text("Configuration is stored as JSON independently from the UI.")
    }
}