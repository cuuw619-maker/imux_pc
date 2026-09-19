package com.imux.launcher

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Build
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Dns
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.Gamepad
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Speed
import androidx.compose.material.icons.filled.Storage
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material3.AssistChip
import androidx.compose.material3.Badge
import androidx.compose.material3.BadgedBox
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Divider
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationDrawerItem
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.imux.core.model.InstallationProfile
import com.imux.core.service.ConfigRepository
import com.imux.core.service.GameLaunchService
import com.imux.core.service.RuntimeValidator
import com.imux.runtime.JsonConfigRepository
import com.imux.runtime.MockGameLaunchService
import com.imux.runtime.RuntimeValidatorImpl
import com.imux.core.state.LauncherUiState
import com.imux.runtime.ImuxPaths
import com.imux.runtime.LauncherMetadataStore
import kotlinx.coroutines.launch

private val ImuxPrimary = Color(0xFF9DEFC9)
private val ImuxPrimaryContainer = Color(0xFF07513C)
private val ImuxSurface = Color(0xFF0D1117)
private val ImuxSurfaceContainer = Color(0xFF171C23)
private val ImuxSurfaceHigh = Color(0xFF20262F)
private val ImuxOutline = Color(0xFF3D4652)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun LauncherApp() {
    val scheme = darkColorScheme(
        primary = ImuxPrimary,
        onPrimary = Color(0xFF00382A),
        primaryContainer = ImuxPrimaryContainer,
        onPrimaryContainer = Color(0xFFB9FFDE),
        secondary = Color(0xFFB8CCC2),
        onSecondary = Color(0xFF24332D),
        secondaryContainer = Color(0xFF394A43),
        onSecondaryContainer = Color(0xFFD4E8DF),
        tertiary = Color(0xFFBBD0FF),
        onTertiary = Color(0xFF21304A),
        background = ImuxSurface,
        onBackground = Color(0xFFE1E5EA),
        surface = ImuxSurface,
        onSurface = Color(0xFFE1E5EA),
        surfaceVariant = ImuxSurfaceHigh,
        onSurfaceVariant = Color(0xFFB7C0CB),
        surfaceContainer = ImuxSurfaceContainer,
        surfaceContainerHigh = ImuxSurfaceHigh,
        outline = ImuxOutline
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

        val play: () -> Unit = {
            if (state is LauncherUiState.Ready) {
                val ready = state as LauncherUiState.Ready
                state = LauncherUiState.Launching(ready.profile)
                scope.launch {
                    val result = gameLauncher.launch(ready.profile)
                    state = if (result.started) {
                        LauncherUiState.Running(ready.profile, result.processId)
                    } else {
                        LauncherUiState.Error(result.message)
                    }
                }
            }
        }

        Box(Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)) {
            Scaffold(
                containerColor = MaterialTheme.colorScheme.background,
                topBar = {
                    TopAppBar(
                        title = {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Surface(
                                    modifier = Modifier.size(34.dp),
                                    shape = RoundedCornerShape(10.dp),
                                    color = MaterialTheme.colorScheme.primaryContainer
                                ) {
                                    Box(contentAlignment = Alignment.Center) {
                                        Icon(Icons.Default.Gamepad, null, tint = MaterialTheme.colorScheme.onPrimaryContainer)
                                    }
                                }
                                Spacer(Modifier.width(12.dp))
                                Column {
                                    Text("Imux", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold)
                                    Text(
                                        page.title,
                                        style = MaterialTheme.typography.labelMedium,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant
                                    )
                                }
                            }
                        },
                        navigationIcon = {
                            IconButton(onClick = { drawer = true }) {
                                Icon(Icons.Default.Menu, "Open navigation")
                            }
                        },
                        colors = TopAppBarDefaults.topAppBarColors(
                            containerColor = MaterialTheme.colorScheme.background
                        )
                    )
                }
            ) { insets ->
                Box(
                    Modifier.fillMaxSize().padding(insets)
                ) {
                    AnimatedContent(
                        targetState = page,
                        transitionSpec = {
                            fadeIn(tween(180)) togetherWith fadeOut(tween(120))
                        },
                        label = "page"
                    ) { target ->
                        when (target) {
                            Page.Home -> HomeScreen(state, play)
                            Page.Instances -> InstancesScreen()
                            Page.Engine -> EngineScreen()
                            Page.Settings -> SettingsScreen()
                        }
                    }

                    AnimatedVisibility(
                        visible = drawer,
                        enter = fadeIn(tween(180)),
                        exit = fadeOut(tween(140))
                    ) {
                        Box(
                            Modifier.fillMaxSize()
                                .background(Color.Black.copy(alpha = 0.55f))
                                .clickable(
                                    indication = null,
                                    interactionSource = remember { MutableInteractionSource() }
                                ) { drawer = false }
                        )
                    }

                    AnimatedVisibility(
                        visible = drawer,
                        enter = slideInHorizontally(
                            initialOffsetX = { -it },
                            animationSpec = tween(260, easing = FastOutSlowInEasing)
                        ) + fadeIn(tween(180)),
                        exit = slideOutHorizontally(
                            targetOffsetX = { -it },
                            animationSpec = tween(220, easing = FastOutSlowInEasing)
                        ) + fadeOut(tween(140))
                    ) {
                        NavigationDrawer(page) { selected ->
                            page = selected
                            drawer = false
                        }
                    }
                }
            }
        }
    }
}

private enum class Page(val title: String, val icon: androidx.compose.ui.graphics.vector.ImageVector) {
    Home("Home", Icons.Default.Home),
    Instances("Instances", Icons.Default.Dns),
    Engine("Engine", Icons.Default.Speed),
    Settings("Settings", Icons.Default.Settings)
}

@Composable
private fun NavigationDrawer(page: Page, onPage: (Page) -> Unit) {
    Surface(
        modifier = Modifier.fillMaxHeight().widthIn(min = 292.dp, max = 360.dp),
        color = MaterialTheme.colorScheme.surfaceContainer,
        shape = RoundedCornerShape(topEnd = 28.dp, bottomEnd = 28.dp),
        tonalElevation = 8.dp
    ) {
        Column(Modifier.fillMaxSize().padding(18.dp)) {
            Row(
                Modifier.fillMaxWidth().padding(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Surface(
                    modifier = Modifier.size(44.dp),
                    shape = RoundedCornerShape(14.dp),
                    color = MaterialTheme.colorScheme.primaryContainer
                ) {
                    Box(contentAlignment = Alignment.Center) {
                        Icon(Icons.Default.Gamepad, null, tint = MaterialTheme.colorScheme.onPrimaryContainer)
                    }
                }
                Spacer(Modifier.width(12.dp))
                Column(Modifier.weight(1f)) {
                    Text("IMUX", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
                    Text("Windows launcher", style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                IconButton(onClick = { onPage(page) }) {
                    Icon(Icons.Default.Close, "Close navigation")
                }
            }

            Spacer(Modifier.height(20.dp))
            Text(
                "WORKSPACE",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(horizontal = 14.dp, vertical = 6.dp)
            )

            Page.entries.forEach { item ->
                NavigationDrawerItem(
                    icon = { Icon(item.icon, null) },
                    label = { Text(item.title) },
                    selected = item == page,
                    onClick = { onPage(item) },
                    shape = RoundedCornerShape(16.dp),
                    modifier = Modifier.padding(vertical = 3.dp)
                )
            }

            Spacer(Modifier.weight(1f))

            Surface(
                Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(18.dp),
                color = MaterialTheme.colorScheme.surfaceVariant
            ) {
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                    Text("DEVELOPMENT BUILD", style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.primary)
                    Text("Windows x64", style = MaterialTheme.typography.bodyMedium)
                    Text("Imux launcher shell", style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
            Spacer(Modifier.height(8.dp))
        }
    }
}

@Composable
private fun LauncherContent(
    content: @Composable ColumnScope.() -> Unit
) {
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val side = when {
            maxWidth < 620.dp -> 16.dp
            maxWidth < 1000.dp -> 24.dp
            else -> 40.dp
        }
        LazyColumn(
            modifier = Modifier.fillMaxSize(),
            contentPadding = androidx.compose.foundation.layout.PaddingValues(
                start = side, end = side, top = 12.dp, bottom = 112.dp
            ),
            verticalArrangement = Arrangement.spacedBy(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            item {
                Column(
                    Modifier.widthIn(max = 1180.dp).fillMaxWidth(),
                    verticalArrangement = Arrangement.spacedBy(4.dp)
                ) {
                    content()
                }
            }
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

    LauncherContent {
        Column(verticalArrangement = Arrangement.spacedBy(18.dp)) {
            Column(verticalArrangement = Arrangement.spacedBy(5.dp)) {
                Text("Welcome back", style = MaterialTheme.typography.headlineLarge, fontWeight = FontWeight.SemiBold)
                Text(
                    "Your Imux environment is ready for development.",
                    style = MaterialTheme.typography.bodyLarge,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }

            BoxWithConstraints(Modifier.fillMaxWidth()) {
                if (maxWidth >= 820.dp) {
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                        Box(Modifier.weight(1.45f)) { ProfileHero(profile, state) }
                        Box(Modifier.weight(1f)) { RuntimeCard(state) }
                    }
                } else {
                    Column(verticalArrangement = Arrangement.spacedBy(16.dp)) {
                        ProfileHero(profile, state)
                        RuntimeCard(state)
                    }
                }
            }

            SectionTitle("Launcher")
            QuickActionCard()
            EnvironmentGrid()

            SectionTitle("Engine status")
            EngineStatusCard()
        }
    }
}

@Composable
private fun ProfileHero(profile: InstallationProfile?, state: LauncherUiState) {
    val running = state is LauncherUiState.Running
    val scale by animateFloatAsState(
        targetValue = if (running) 1.02f else 1f,
        animationSpec = tween(450, easing = FastOutSlowInEasing),
        label = "profile-scale"
    )

    Card(
        Modifier.fillMaxWidth().graphicsLayer { scaleX = scale; scaleY = scale },
        shape = RoundedCornerShape(28.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerHigh)
    ) {
        Column(Modifier.padding(24.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Surface(
                    Modifier.size(52.dp),
                    shape = RoundedCornerShape(16.dp),
                    color = MaterialTheme.colorScheme.primaryContainer
                ) {
                    Box(contentAlignment = Alignment.Center) {
                        Icon(Icons.Default.Gamepad, null, tint = MaterialTheme.colorScheme.onPrimaryContainer)
                    }
                }
                Spacer(Modifier.width(14.dp))
                Column(Modifier.weight(1f)) {
                    Text("ACTIVE PROFILE", style = MaterialTheme.typography.labelMedium,
                        color = MaterialTheme.colorScheme.primary)
                    Text(
                        profile?.name ?: "Loading…",
                        style = MaterialTheme.typography.headlineSmall,
                        fontWeight = FontWeight.SemiBold,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                }
                StatusBadge(state)
            }
            Text(
                "Windows x64  •  " + (profile?.version ?: "—"),
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Surface(
                Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(16.dp),
                color = MaterialTheme.colorScheme.surfaceVariant
            ) {
                Row(
                    Modifier.padding(horizontal = 16.dp, vertical = 13.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Icon(Icons.Default.Memory, null, tint = MaterialTheme.colorScheme.primary)
                    Spacer(Modifier.width(12.dp))
                    Column {
                        Text("Memory profile", style = MaterialTheme.typography.labelMedium)
                        Text("1–4 GB allocated", style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                }
            }
        }
    }
}

@Composable
private fun StatusBadge(state: LauncherUiState) {
    val text = when (state) {
        is LauncherUiState.Ready -> if (state.runtimeAvailable) "READY" else "RUNTIME"
        is LauncherUiState.Launching -> "STARTING"
        is LauncherUiState.Running -> "RUNNING"
        is LauncherUiState.Error -> "ERROR"
        LauncherUiState.Loading -> "LOADING"
    }
    Surface(
        shape = RoundedCornerShape(50),
        color = if (state is LauncherUiState.Error) MaterialTheme.colorScheme.errorContainer
        else MaterialTheme.colorScheme.primaryContainer
    ) {
        Text(
            text,
            modifier = Modifier.padding(horizontal = 10.dp, vertical = 5.dp),
            style = MaterialTheme.typography.labelSmall,
            color = if (state is LauncherUiState.Error) MaterialTheme.colorScheme.onErrorContainer
            else MaterialTheme.colorScheme.onPrimaryContainer
        )
    }
}

@Composable
private fun RuntimeCard(state: LauncherUiState) {
    Card(
        Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(28.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer)
    ) {
        Column(Modifier.padding(24.dp), verticalArrangement = Arrangement.spacedBy(16.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Surface(Modifier.size(42.dp), CircleShape, color = MaterialTheme.colorScheme.surfaceVariant) {
                    Box(contentAlignment = Alignment.Center) {
                        Icon(Icons.Default.Speed, null, tint = MaterialTheme.colorScheme.primary)
                    }
                }
                Spacer(Modifier.width(12.dp))
                Column {
                    Text("Runtime", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
                    Text("Java environment", style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
            RuntimeLine("Java", "21")
            RuntimeLine("Platform", "Windows x64")
            RuntimeLine("Mods", "None")
            RuntimeLine("Status", if (state is LauncherUiState.Ready && state.runtimeAvailable) "Available" else statusText(state))
        }
    }
}

@Composable
private fun RuntimeLine(label: String, value: String) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(label, Modifier.weight(1f), color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(value, fontWeight = FontWeight.Medium, maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}

@Composable
private fun SectionTitle(title: String) {
    Row(Modifier.fillMaxWidth().padding(top = 2.dp), verticalAlignment = Alignment.CenterVertically) {
        Text(title, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
        Spacer(Modifier.weight(1f))
        Divider(Modifier.width(70.dp))
    }
}

@Composable
private fun QuickActionCard() {
    Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(22.dp)) {
        Row(
            Modifier.fillMaxWidth().padding(18.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Surface(Modifier.size(42.dp), RoundedCornerShape(13.dp),
                color = MaterialTheme.colorScheme.primaryContainer) {
                Box(contentAlignment = Alignment.Center) {
                    Icon(Icons.Default.Folder, null, tint = MaterialTheme.colorScheme.onPrimaryContainer)
                }
            }
            Spacer(Modifier.width(14.dp))
            Column(Modifier.weight(1f)) {
                Text("Game directory", fontWeight = FontWeight.Medium)
                Text(
                    "%APPDATA%\\Imux",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    style = MaterialTheme.typography.bodySmall,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }
            AssistChip(onClick = {}, label = { Text("DEFAULT") })
        }
    }
}

@Composable
private fun EnvironmentGrid() {
    BoxWithConstraints(Modifier.fillMaxWidth()) {
        val compact = maxWidth < 700.dp
        if (compact) {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                MetricCard("Memory", "1–4 GB", Icons.Default.Memory)
                MetricCard("Renderer", "D3D11", Icons.Default.Speed)
                MetricCard("Storage", "SQLite", Icons.Default.Storage)
            }
        } else {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Box(Modifier.weight(1f)) { MetricCard("Memory", "1–4 GB", Icons.Default.Memory) }
                Box(Modifier.weight(1f)) { MetricCard("Renderer", "D3D11", Icons.Default.Speed) }
                Box(Modifier.weight(1f)) { MetricCard("Storage", "SQLite", Icons.Default.Storage) }
            }
        }
    }
}

@Composable
private fun MetricCard(label: String, value: String, icon: androidx.compose.ui.graphics.vector.ImageVector) {
    Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(20.dp)) {
        Column(Modifier.padding(18.dp), verticalArrangement = Arrangement.spacedBy(9.dp)) {
            Icon(icon, null, tint = MaterialTheme.colorScheme.primary)
            Text(label, style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(value, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Medium)
        }
    }
}

@Composable
private fun EngineStatusCard() {
    Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(22.dp)) {
        Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            EngineRow("Graphics API", "Direct3D 11")
            EngineRow("Shader", "HLSL")
            EngineRow("Interop", "C / Rust boundary")
            EngineRow("World", "First-person test scene")
        }
    }
}

@Composable
private fun EngineRow(label: String, value: String) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(label, Modifier.weight(1f), color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(value, Modifier.widthIn(max = 190.dp), maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}

@Composable
private fun InstancesScreen() {
    LauncherContent {
        ScreenHeader("Instances", "Separate environments for the Imux client.")
        InstanceCard()
        SectionTitle("Storage")
        InfoCard("Instance data", "%APPDATA%\\Imux", "Configuration and launcher metadata are kept outside the renderer.")
    }
}

@Composable
private fun InstanceCard() {
    Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(26.dp)) {
        Column(Modifier.padding(22.dp), verticalArrangement = Arrangement.spacedBy(16.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Surface(Modifier.size(48.dp), RoundedCornerShape(15.dp),
                    color = MaterialTheme.colorScheme.primaryContainer) {
                    Box(contentAlignment = Alignment.Center) {
                        Icon(Icons.Default.Dns, null, tint = MaterialTheme.colorScheme.onPrimaryContainer)
                    }
                }
                Spacer(Modifier.width(13.dp))
                Column(Modifier.weight(1f)) {
                    Text("Default Client", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold)
                    Text("Development profile", color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                AssistChip(onClick = {}, label = { Text("ACTIVE") })
            }
            Divider()
            BoxWithConstraints(Modifier.fillMaxWidth()) {
                if (maxWidth < 620.dp) {
                    Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                        RuntimeLine("Runtime", "Java 21")
                        RuntimeLine("Memory", "1–4 GB")
                        RuntimeLine("Mods", "None")
                    }
                } else {
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(30.dp)) {
                        RuntimeLine("Runtime", "Java 21")
                        RuntimeLine("Memory", "1–4 GB")
                        RuntimeLine("Mods", "None")
                    }
                }
            }
        }
    }
}

@Composable
private fun EngineScreen() {
    LauncherContent {
        ScreenHeader("Engine", "Native rendering and world systems.")
        EngineDetail("Rendering", "D3D11 device, swap chain, depth buffer, first-person camera and HLSL pipeline.", Icons.Default.Speed)
        EngineDetail("World", "Block test scene with collision floor, gravity, WASD movement, sprint and mouse look.", Icons.Default.Gamepad)
        EngineDetail("Assets", "Block, GUI and panorama resources are packaged with the engine.", Icons.Default.Folder)
        EngineDetail("Interop", "C ABI boundary keeps Rust systems and Kotlin launcher services separate from rendering.", Icons.Default.Build)
        EngineDetail("Next", "Chunk storage, block registry, texture atlas, culling, raycast and real world interaction.", Icons.Default.Tune)
    }
}

@Composable
private fun EngineDetail(title: String, value: String, icon: androidx.compose.ui.graphics.vector.ImageVector) {
    Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(22.dp)) {
        Row(Modifier.padding(20.dp), verticalAlignment = Alignment.Top) {
            Surface(Modifier.size(44.dp), RoundedCornerShape(14.dp),
                color = MaterialTheme.colorScheme.surfaceVariant) {
                Box(contentAlignment = Alignment.Center) {
                    Icon(icon, null, tint = MaterialTheme.colorScheme.primary)
                }
            }
            Spacer(Modifier.width(14.dp))
            Column(verticalArrangement = Arrangement.spacedBy(5.dp)) {
                Text(title, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
                Text(value, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}

@Composable
private fun SettingsScreen() {
    LauncherContent {
        ScreenHeader("Settings", "Configuration without burying the important controls.")
        SettingsSection("Appearance", "Material 3 dark theme, adaptive layout and animated navigation.", Icons.Default.Tune)
        SettingsSection("Runtime", "Java 21 validation and launch service configuration.", Icons.Default.Speed)
        SettingsSection("Storage", "%APPDATA%\\Imux, JSON configuration and SQLite metadata.", Icons.Default.Storage)
        SettingsSection("Assets", "Native GUI, block textures and panorama resources.", Icons.Default.Folder)
        SettingsSection("Advanced", "Renderer diagnostics, shader backend selection and development options.", Icons.Default.Build)
    }
}

@Composable
private fun SettingsSection(title: String, value: String, icon: androidx.compose.ui.graphics.vector.ImageVector) {
    Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(22.dp)) {
        Row(Modifier.padding(20.dp), verticalAlignment = Alignment.CenterVertically) {
            Icon(icon, null, tint = MaterialTheme.colorScheme.primary)
            Spacer(Modifier.width(14.dp))
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text(title, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
                Text(value, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}

@Composable
private fun InfoCard(title: String, value: String, description: String) {
    Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(22.dp)) {
        Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(5.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
            Text(value, color = MaterialTheme.colorScheme.primary)
            Text(description, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Composable
private fun ScreenHeader(title: String, subtitle: String) {
    Column(Modifier.fillMaxWidth().padding(bottom = 2.dp), verticalArrangement = Arrangement.spacedBy(5.dp)) {
        Text(title, style = MaterialTheme.typography.headlineLarge, fontWeight = FontWeight.SemiBold)
        Text(subtitle, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}

private fun statusText(state: LauncherUiState): String = when (state) {
    is LauncherUiState.Ready -> if (state.runtimeAvailable) "Services ready" else "Runtime unavailable"
    is LauncherUiState.Launching -> "Starting development runtime…"
    is LauncherUiState.Running -> "Runtime started • PID " + (state.pid ?: "unknown")
    is LauncherUiState.Error -> state.message
    LauncherUiState.Loading -> "Loading configuration…"
}
