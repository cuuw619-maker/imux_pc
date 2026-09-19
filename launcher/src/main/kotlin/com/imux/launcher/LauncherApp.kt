package com.imux.launcher

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
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
        secondary = Color(0xFFB8CCC2), background = Color(0xFF0B0D11),
        surface = Color(0xFF14171D), surfaceContainer = Color(0xFF1B1F27),
        surfaceVariant = Color(0xFF232832), outline = Color(0xFF3B424E)
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

        Box(Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)) {
            Column(Modifier.fillMaxSize()) {
                TopAppBar(
                    title = {
                        Column {
                            Text("Imux", style = MaterialTheme.typography.titleLarge)
                            Text(page.title, style = MaterialTheme.typography.labelSmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                    },
                    navigationIcon = {
                        IconButton(onClick = { drawer = true }) {
                            Icon(Icons.Default.Menu, contentDescription = "Open navigation")
                        }
                    }
                )
                Box(Modifier.weight(1f).fillMaxWidth()) {
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

                    AnimatedVisibility(visible = drawer, enter = fadeIn(), exit = fadeOut()) {
                        Box(Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.52f))
                            .clickable { drawer = false })
                    }
                    AnimatedVisibility(
                        visible = drawer,
                        enter = slideInHorizontally { -it } + fadeIn(),
                        exit = slideOutHorizontally { -it } + fadeOut()
                    ) {
                        NavigationDrawer(page) { page = it; drawer = false }
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
private fun NavigationDrawer(page: Page, onPage: (Page) -> Unit) {
    ModalDrawerSheet(
        modifier = Modifier.fillMaxHeight().widthIn(min = 280.dp, max = 340.dp),
        drawerContainerColor = MaterialTheme.colorScheme.surfaceContainer
    ) {
        Spacer(Modifier.height(24.dp))
        Text("IMUX", style = MaterialTheme.typography.headlineMedium,
            modifier = Modifier.padding(horizontal = 24.dp, vertical = 12.dp))
        Text("Launcher", color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(horizontal = 24.dp, vertical = 4.dp))
        Spacer(Modifier.height(18.dp))
        Page.entries.forEach { item ->
            NavigationDrawerItem(
                label = { Text(item.title) },
                selected = item == page,
                onClick = { onPage(item) },
                modifier = Modifier.padding(horizontal = 12.dp, vertical = 4.dp)
            )
        }
        Spacer(Modifier.weight(1f))
        Text("Windows x64", style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(24.dp))
    }
}

@Composable
private fun ScreenFrame(
    title: String,
    subtitle: String,
    content: @Composable ColumnScope.() -> Unit
) {
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val horizontal = if (maxWidth < 760.dp) 16.dp else if (maxWidth < 1100.dp) 24.dp else 40.dp
        Column(
            Modifier.fillMaxSize().widthIn(max = 1220.dp).align(Alignment.TopCenter)
                .padding(horizontal = horizontal, vertical = 20.dp)
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Text(title, style = MaterialTheme.typography.displaySmall)
            Text(subtitle, color = MaterialTheme.colorScheme.onSurfaceVariant)
            content()
            Spacer(Modifier.height(8.dp))
        }
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
    ScreenFrame("Your game environment",
        "A clean control surface for the Imux client.") {
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            if (maxWidth < 860.dp) {
                Column(verticalArrangement = Arrangement.spacedBy(16.dp)) {
                    ProfileCard(profile, state)
                    QuickStatus()
                }
            } else {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(18.dp)) {
                    Box(Modifier.weight(1.55f)) { ProfileCard(profile, state) }
                    Box(Modifier.weight(1f)) { QuickStatus() }
                }
            }
        }
        Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(24.dp)) {
            Column(Modifier.padding(22.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Test environment", style = MaterialTheme.typography.titleMedium)
                Text("The PLAY action starts the current development runtime. The 3D renderer is being developed as a separate native engine.",
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
            FilledTonalButton(
                onClick = onPlay,
                enabled = state is LauncherUiState.Ready,
                contentPadding = PaddingValues(horizontal = 30.dp, vertical = 15.dp)
            ) {
                Icon(Icons.Default.PlayArrow, contentDescription = null)
                Spacer(Modifier.width(8.dp))
                Text(if (state is LauncherUiState.Launching) "STARTING" else "PLAY")
            }
        }
    }
}

@Composable
private fun ProfileCard(profile: InstallationProfile?, state: LauncherUiState) {
    Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(28.dp)) {
        Column(Modifier.padding(28.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("ACTIVE PROFILE", style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.primary)
            Text(profile?.name ?: "Loading…", style = MaterialTheme.typography.headlineMedium)
            Text("Windows x64  •  " + (profile?.version ?: "—"),
                color = MaterialTheme.colorScheme.onSurfaceVariant)
            Spacer(Modifier.height(16.dp))
            Surface(Modifier.fillMaxWidth(), shape = RoundedCornerShape(16.dp),
                color = MaterialTheme.colorScheme.surfaceVariant) {
                Row(Modifier.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text("STATUS", style = MaterialTheme.typography.labelMedium,
                        color = MaterialTheme.colorScheme.primary)
                    Spacer(Modifier.width(18.dp))
                    Text(statusText(state), color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
        }
    }
}

private fun statusText(state: LauncherUiState) = when (state) {
    is LauncherUiState.Ready -> if (state.runtimeAvailable) "Services ready" else "Runtime unavailable"
    is LauncherUiState.Launching -> "Starting development runtime…"
    is LauncherUiState.Running -> "Runtime started • PID " + (state.pid ?: "unknown")
    is LauncherUiState.Error -> state.message
    LauncherUiState.Loading -> "Loading configuration…"
}

@Composable
private fun QuickStatus() {
    Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(28.dp)) {
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
private fun InstancesScreen() {
    ScreenFrame("Instances", "Manage isolated development environments.") {
        InstanceCard("Default Client", "Development profile", "Java 21", "1–4 GB")
        Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(22.dp)) {
            Column(Modifier.padding(22.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Text("Instance data", style = MaterialTheme.typography.titleMedium)
                Text("%APPDATA%\\Imux", color = MaterialTheme.colorScheme.onSurfaceVariant)
                Text("JSON configuration and SQLite metadata are stored separately from the renderer.",
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}

@Composable
private fun InstanceCard(name: String, description: String, runtime: String, memory: String) {
    Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(26.dp)) {
        Column(Modifier.padding(24.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text(name, style = MaterialTheme.typography.titleLarge)
                    Text(description, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                AssistChip(onClick = {}, label = { Text("ACTIVE") })
            }
            HorizontalDivider(Modifier.padding(vertical = 8.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(32.dp)) {
                StatusLine("Runtime", runtime)
                StatusLine("Memory", memory)
                StatusLine("Mods", "None")
            }
        }
    }
}

@Composable
private fun EngineScreen() {
    ScreenFrame("Engine", "The native side is now treated as an engine, not a launcher demo.") {
        EngineSection("Rendering", "D3D11 device, swap chain, depth buffer, first-person camera and HLSL pipeline.")
        EngineSection("World", "Explicit block scene with collision floor, gravity, WASD movement, sprint and mouse look. No terrain generation yet.")
        EngineSection("Assets", "Block textures, GUI textures and panorama assets are organized under the native asset tree.")
        EngineSection("Interop", "C ABI boundary with Rust systems and Kotlin launcher services kept separate from rendering.")
        EngineSection("Next layer", "Chunk storage, block registry, texture atlas, frustum culling, ray casting and real world interaction.")
    }
}

@Composable
private fun EngineSection(title: String, value: String) {
    Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(22.dp)) {
        Column(Modifier.padding(22.dp), verticalArrangement = Arrangement.spacedBy(7.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.primary)
            Text(value, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Composable
private fun SettingsScreen() {
    ScreenFrame("Settings", "Launcher configuration and developer controls.") {
        SettingsCard("Appearance", "Material You inspired theme, adaptive density and animated navigation.")
        SettingsCard("Runtime", "Java 21 validation and launch service configuration.")
        SettingsCard("Storage", "%APPDATA%\\Imux, JSON configuration and SQLite metadata.")
        SettingsCard("Assets", "Native GUI, block textures and panorama resources are packaged with the engine.")
        SettingsCard("Advanced", "Renderer diagnostics, shader backend selection and engine development options.")
    }
}

@Composable
private fun SettingsCard(title: String, value: String) {
    Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(22.dp)) {
        Column(Modifier.padding(22.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium)
            Text(value, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}
