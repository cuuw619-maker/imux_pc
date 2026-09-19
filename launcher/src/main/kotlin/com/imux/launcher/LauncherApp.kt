package com.imux.launcher

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material3.*
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.imux.core.model.InstallationProfile
import com.imux.core.service.*
import com.imux.core.state.LauncherUiState
import com.imux.runtime.*
import kotlinx.coroutines.launch

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun LauncherApp() {
    val scheme = darkColorScheme(
        primary = Color(0xFF8DE6C0), onPrimary = Color(0xFF00382A),
        secondary = Color(0xFFB8CCC2), background = Color(0xFF0F1115),
        surface = Color(0xFF171A20), surfaceContainer = Color(0xFF1D2128)
    )
    MaterialTheme(colorScheme = scheme) {
        val repository: ConfigRepository = remember { JsonConfigRepository() }
        val validator: RuntimeValidator = remember { RuntimeValidatorImpl() }
        val gameLauncher: GameLaunchService = remember { MockGameLaunchService() }
        val metadataStore = remember { LauncherMetadataStore() }
        val scope = rememberCoroutineScope()
        var state by remember { mutableStateOf<LauncherUiState>(LauncherUiState.Loading) }
        var page by remember { mutableStateOf(Page.Home) }
        var drawer by remember { mutableStateOf(false) }

        LaunchedEffect(Unit) {
            runCatching { metadataStore.initialize() }
            runCatching {
                val config = repository.load()
                val profile = config.installations.firstOrNull { it.id == config.selectedInstallationId }
                    ?: config.installations.first()
                val runtime = validator.validate(config.runtime)
                LauncherUiState.Ready(profile, runtime.available)
            }.onSuccess { state = it }
             .onFailure { state = LauncherUiState.Error(it.message ?: "Unable to load launcher configuration.") }
        }

        BoxWithConstraints(Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)) {
            val compact = maxWidth < 900.dp
            Column(Modifier.fillMaxSize()) {
                TopAppBar(
                    title = { Text(page.title) },
                    navigationIcon = {
                        IconButton(onClick = { drawer = true }) {
                            Icon(Icons.Default.Menu, contentDescription = "Open navigation")
                        }
                    }
                )
                Box(Modifier.fillMaxSize()) {
                    when (page) {
                        Page.Home -> HomeScreen(state) {
                            val ready = state as? LauncherUiState.Ready ?: return@HomeScreen
                            state = LauncherUiState.Launching(ready.profile)
                            scope.launch {
                                val result = gameLauncher.launch(ready.profile)
                                state = if (result.started) LauncherUiState.Running(ready.profile, result.processId)
                                else LauncherUiState.Error(result.message)
                            }
                        }
                        Page.Instances -> InstancesScreen()
                        Page.Engine -> EngineScreen()
                        Page.Settings -> SettingsScreen()
                    }
                    androidx.compose.animation.AnimatedVisibility(visible = drawer, enter = fadeIn(), exit = fadeOut()) {
                        Box(
                            Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.42f))
                                .clickable { drawer = false }
                        )
                    }
                    androidx.compose.animation.AnimatedVisibility(
                        visible = drawer,
                        enter = slideInHorizontally { -it } + fadeIn(),
                        exit = slideOutHorizontally { -it } + fadeOut()
                    ) {
                        NavigationDrawer(page, compact) { page = it; drawer = false }
                    }
                }
            }
        }
    }
}

private enum class Page(val title: String) {
    Home("Home"), Instances("Instances"), Engine("Engine"), Settings("Settings")
}

@Composable
private fun NavigationDrawer(page: Page, compact: Boolean, onPage: (Page) -> Unit) {
    ModalDrawerSheet(
        modifier = Modifier.width(if (compact) 300.dp else 320.dp),
        drawerContainerColor = MaterialTheme.colorScheme.surfaceContainer
    ) {
        Spacer(Modifier.height(18.dp))
        Text("IMUX", style = MaterialTheme.typography.headlineMedium,
            modifier = Modifier.padding(horizontal = 24.dp, vertical = 14.dp))
        Page.entries.forEach { item ->
            NavigationDrawerItem(
                label = { Text(item.title) }, selected = item == page,
                onClick = { onPage(item) }, modifier = Modifier.padding(horizontal = 12.dp, vertical = 3.dp)
            )
        }
        Spacer(Modifier.weight(1f))
        Text("Windows x64", style = MaterialTheme.typography.labelSmall,
            modifier = Modifier.padding(24.dp))
    }
}

@Composable
private fun ScreenFrame(content: @Composable ColumnScope.() -> Unit) {
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val horizontal = if (maxWidth < 1000.dp) 20.dp else 32.dp
        Column(
            Modifier.fillMaxWidth().widthIn(max = 1180.dp).align(Alignment.TopCenter)
                .padding(horizontal = horizontal, vertical = 20.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp), content = content
        )
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
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val compact = maxWidth < 900.dp
        ScreenFrame {
            Text("Your game environment", style = MaterialTheme.typography.headlineLarge)
            Text(
                "One clear launch action. Everything else stays out of the way.",
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )

            if (compact) {
                ProfileCard(profile, state)
            } else {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                    Box(Modifier.weight(1.6f)) { ProfileCard(profile, state) }
                    QuickStatus()
                }
            }

            Spacer(Modifier.weight(1f))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                FilledTonalButton(
                    onClick = onPlay,
                    enabled = state is LauncherUiState.Ready,
                    contentPadding = PaddingValues(horizontal = 28.dp, vertical = 14.dp)
                ) {
                    Icon(Icons.Default.PlayArrow, contentDescription = null)
                    Spacer(Modifier.width(8.dp))
                    Text(if (state is LauncherUiState.Launching) "STARTING" else "PLAY")
                }
            }
        }
    }
}

@Composable
private fun ProfileCard(profile: InstallationProfile?, state: LauncherUiState) {
    Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(28.dp)) {
        Column(
            Modifier.fillMaxWidth().padding(28.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Text("ACTIVE PROFILE", style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.primary)
            Text(profile?.name ?: "Loading…", style = MaterialTheme.typography.headlineSmall)
            Text(
                "Windows x64  •  " + (profile?.version ?: "—"),
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Spacer(Modifier.height(18.dp))
            Surface(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(16.dp),
                color = MaterialTheme.colorScheme.surfaceVariant
            ) {
                Row(Modifier.padding(horizontal = 18.dp, vertical = 14.dp),
                    verticalAlignment = Alignment.CenterVertically) {
                    Text("READY", color = MaterialTheme.colorScheme.primary,
                        style = MaterialTheme.typography.labelMedium)
                    Spacer(Modifier.width(18.dp))
                    Text(
                        when (state) {
                            is LauncherUiState.Ready -> if (state.runtimeAvailable) "Services ready" else "Runtime unavailable"
                            is LauncherUiState.Launching -> "Starting test world…"
                            is LauncherUiState.Running -> "Test world running"
                            is LauncherUiState.Error -> state.message
                            LauncherUiState.Loading -> "Loading configuration…"
                        },
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
        }
    }
}

@Composable
private fun QuickStatus() {
    Card(Modifier.weight(1f).fillMaxHeight(), shape = RoundedCornerShape(28.dp)) {
        Column(Modifier.padding(24.dp), verticalArrangement = Arrangement.spacedBy(18.dp)) {
            Text("Quick status", style = MaterialTheme.typography.titleMedium)
            StatusLine("Runtime", "Java 21")
            StatusLine("Memory", "1–4 GB")
            StatusLine("Mods", "None")
        }
    }
}

@Composable
private fun StatusLine(label: String, value: String) {
    Column(verticalArrangement = Arrangement.spacedBy(3.dp)) {
        Text(label, style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(value, style = MaterialTheme.typography.bodyLarge)
    }
}

@Composable
private fun ProfileInfo(profile: InstallationProfile?, state: LauncherUiState) {
    Text(profile?.name ?: "Loading…", style = MaterialTheme.typography.headlineSmall)
    Text("Windows x64  •  " + (profile?.version ?: "—"),
        color = MaterialTheme.colorScheme.onSurfaceVariant)
    Spacer(Modifier.height(8.dp))
    val status = when (state) {
        is LauncherUiState.Ready -> if (state.runtimeAvailable) "Runtime ready" else "Runtime unavailable"
        is LauncherUiState.Launching -> "Starting development runtime…"
        is LauncherUiState.Running -> "Runtime started • PID " + (state.pid ?: "unknown")
        is LauncherUiState.Error -> state.message
        LauncherUiState.Loading -> "Loading configuration…"
    }
    Text(status, color = MaterialTheme.colorScheme.primary)
}

@Composable
private fun InstancesScreen() {
    ScreenFrame {
        Text("Instances", style = MaterialTheme.typography.headlineLarge)
        Text("Isolated launch environments.", color = MaterialTheme.colorScheme.onSurfaceVariant)
        Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(24.dp)) {
            Column(Modifier.padding(24.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Default Client", style = MaterialTheme.typography.titleLarge)
                Text("Development profile", color = MaterialTheme.colorScheme.onSurfaceVariant)
                Text("Runtime configuration is handled by launcher services.")
            }
        }
    }
}

@Composable
private fun EngineScreen() {
    ScreenFrame {
        Text("Engine", style = MaterialTheme.typography.headlineLarge)
        Text("Native rendering foundation.", color = MaterialTheme.colorScheme.onSurfaceVariant)
        listOf(
            "Renderer" to "Direct2D / DirectWrite + future D3D12 backend",
            "GPU" to "D3D11 foundation + HLSL shader pipeline",
            "Systems" to "C++ / C / Rust",
            "Build" to "CMake + Gradle + GitHub Actions"
        ).forEach { (name, value) ->
            ListItem(headlineContent = { Text(name) }, supportingContent = { Text(value) })
        }
    }
}

@Composable
private fun SettingsScreen() {
    ScreenFrame {
        Text("Settings", style = MaterialTheme.typography.headlineLarge)
        Text("Configure the launcher without exposing implementation details.",
            color = MaterialTheme.colorScheme.onSurfaceVariant)
        SettingsCard("Appearance", "Material You • adaptive layout • motion")
        SettingsCard("Runtime", "Java 21 • automatic runtime validation")
        SettingsCard("Storage", "%APPDATA%\\Imux • JSON configuration • SQLite metadata")
        SettingsCard("Advanced", "Developer diagnostics and technology information")
    }
}

@Composable
private fun SettingsCard(title: String, value: String) {
    Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(20.dp)) {
        Column(Modifier.padding(20.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium)
            Spacer(Modifier.height(4.dp))
            Text(value, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}
