package com.imux.launcher

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.layout.weight
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Launch
import androidx.compose.material.icons.filled.MenuBook
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.VideogameAsset
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Divider
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
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
import androidx.compose.ui.draw.scale
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.imux.core.model.InstallationProfile
import com.imux.core.release.ImuxRelease
import com.imux.core.service.ConfigRepository
import com.imux.core.service.GameLaunchService
import com.imux.core.service.RuntimeValidator
import com.imux.core.state.LauncherUiState
import com.imux.runtime.GameLaunchServiceImpl
import com.imux.runtime.ImuxPaths
import com.imux.runtime.JsonConfigRepository
import com.imux.runtime.RuntimeValidatorImpl
import kotlinx.coroutines.launch

private val LaunchAccent = Color(0xFF9EF1C8)
private val LaunchAccentStrong = Color(0xFF45D99B)
private val LaunchBackground = Color(0xFF080B0F)
private val LaunchSurface = Color(0xFF11161D)
private val LaunchSurface2 = Color(0xFF171D25)
private val LaunchOutline = Color(0xFF2C353F)
private val LaunchText = Color(0xFFE8EDF2)
private val LaunchMuted = Color(0xFF87929F)

@Composable
fun LauncherApp() {
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = LaunchAccent,
            onPrimary = Color(0xFF082016),
            primaryContainer = Color(0xFF153C2D),
            onPrimaryContainer = Color(0xFFB8F8D9),
            secondary = Color(0xFFAAB8C5),
            background = LaunchBackground,
            onBackground = LaunchText,
            surface = LaunchSurface,
            onSurface = LaunchText,
            surfaceVariant = LaunchSurface2,
            onSurfaceVariant = LaunchMuted,
            outline = LaunchOutline
        )
    ) {
        val repository: ConfigRepository = remember { JsonConfigRepository() }
        val validator: RuntimeValidator = remember { RuntimeValidatorImpl() }
        val gameLauncher: GameLaunchService = remember { GameLaunchServiceImpl() }
        val scope = rememberCoroutineScope()

        var state by remember { mutableStateOf<LauncherUiState>(LauncherUiState.Loading) }
        var page by remember { mutableStateOf(Page.Start) }

        LaunchedEffect(Unit) {
            runCatching {
                val config = repository.load()
                val profile = config.installations.firstOrNull { it.id == config.selectedInstallationId }
                    ?: config.installations.firstOrNull()
                    ?: error("No installation profile is configured.")
                val runtime = validator.validate(config.runtime)
                LauncherUiState.Ready(profile, runtime.available)
            }.onSuccess { state = it }
                .onFailure { state = LauncherUiState.Error(it.message ?: "Unable to load launcher state.") }
        }

        val play: () -> Unit = {
            val ready = state as? LauncherUiState.Ready
            if (ready != null) {
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

        Scaffold(
            containerColor = LaunchBackground,
            topBar = { LauncherHeader(page, onPage = { page = it }) }
        ) { insets ->
            Box(
                Modifier.fillMaxSize()
                    .padding(insets)
                    .background(LaunchBackground)
            ) {
                AnimatedContent(
                    targetState = page,
                    transitionSpec = {
                        fadeIn(tween(170)) togetherWith fadeOut(tween(120))
                    },
                    label = "launcher-page"
                ) { target ->
                    when (target) {
                        Page.Start -> StartPage(state, play)
                        Page.Library -> LibraryPage(state)
                        Page.Changelog -> ChangelogPage()
                        Page.Settings -> SettingsPage()
                    }
                }
            }
        }
    }
}

private enum class Page(val label: String, val icon: ImageVector) {
    Start("Start", Icons.Default.Home),
    Library("Library", Icons.Default.VideogameAsset),
    Changelog("Changelog", Icons.Default.MenuBook),
    Settings("Settings", Icons.Default.Settings)
}

@Composable
private fun LauncherHeader(page: Page, onPage: (Page) -> Unit) {
    Surface(color = LaunchBackground, modifier = Modifier.fillMaxWidth()) {
        Row(
            Modifier.fillMaxWidth().height(82.dp).padding(horizontal = 22.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Surface(
                    Modifier.size(42.dp),
                    shape = RoundedCornerShape(13.dp),
                    color = LaunchAccent
                ) {
                    Box(contentAlignment = Alignment.Center) {
                        Icon(Icons.Default.Launch, null, tint = Color(0xFF092017))
                    }
                }
                Spacer(Modifier.width(12.dp))
                Column {
                    Text("IMUX", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
                    Text(
                        ImuxRelease.CURRENT_VERSION,
                        style = MaterialTheme.typography.labelSmall,
                        color = LaunchMuted
                    )
                }
            }

            Spacer(Modifier.width(18.dp))
            Divider(Modifier.height(30.dp).width(1.dp))
            Spacer(Modifier.width(10.dp))

            Row(
                Modifier.weight(1f)
                    .horizontalScroll(rememberScrollState())
                    .padding(horizontal = 4.dp),
                horizontalArrangement = Arrangement.spacedBy(7.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Page.entries.forEach { item ->
                    HeaderNav(item, selected = item == page, onClick = { onPage(item) })
                }
            }

            Spacer(Modifier.width(10.dp))
            Surface(shape = CircleShape, color = LaunchSurface2) {
                Text(
                    "GUEST",
                    modifier = Modifier.padding(horizontal = 13.dp, vertical = 8.dp),
                    style = MaterialTheme.typography.labelSmall,
                    color = LaunchMuted
                )
            }
        }
    }
}

@Composable
private fun HeaderNav(page: Page, selected: Boolean, onClick: () -> Unit) {
    Surface(
        modifier = Modifier.clip(RoundedCornerShape(14.dp)).clickable(onClick = onClick),
        color = if (selected) LaunchSurface2 else Color.Transparent,
        shape = RoundedCornerShape(14.dp)
    ) {
        Row(
            Modifier.padding(horizontal = 14.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(7.dp)
        ) {
            Icon(
                page.icon,
                null,
                tint = if (selected) LaunchAccent else LaunchMuted,
                modifier = Modifier.size(18.dp)
            )
            Text(
                page.label,
                style = MaterialTheme.typography.labelLarge,
                color = if (selected) LaunchText else LaunchMuted
            )
        }
    }
}

@Composable
private fun PageFrame(content: @Composable ColumnScope.() -> Unit) {
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val side = when {
            maxWidth < 620.dp -> 14.dp
            maxWidth < 980.dp -> 22.dp
            else -> 42.dp
        }
        LazyColumn(
            modifier = Modifier.fillMaxSize(),
            contentPadding = PaddingValues(start = side, end = side, top = 24.dp, bottom = 36.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            item {
                Column(
                    Modifier.fillMaxWidth().widthIn(max = 1240.dp),
                    verticalArrangement = Arrangement.spacedBy(16.dp),
                    content = content
                )
            }
        }
    }
}

@Composable
private fun StartPage(state: LauncherUiState, onPlay: () -> Unit) {
    PageFrame {
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            if (maxWidth < 900.dp) {
                Column(verticalArrangement = Arrangement.spacedBy(16.dp)) {
                    LaunchDeck(state, onPlay)
                    LatestReleaseCard()
                }
            } else {
                Row(
                    Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(16.dp),
                    verticalAlignment = Alignment.Top
                ) {
                    Box(Modifier.weight(1.45f)) { LaunchDeck(state, onPlay) }
                    Box(Modifier.weight(0.82f)) { LatestReleaseCard() }
                }
            }
        }
        SoonRail()
    }
}

@Composable
private fun LaunchDeck(state: LauncherUiState, onPlay: () -> Unit) {
    val profile = when (state) {
        is LauncherUiState.Ready -> state.profile
        is LauncherUiState.Launching -> state.profile
        is LauncherUiState.Running -> state.profile
        else -> null
    }
    val running = state is LauncherUiState.Running
    val playScale by animateFloatAsState(
        targetValue = if (running) 0.98f else 1f,
        animationSpec = tween(220, easing = FastOutSlowInEasing),
        label = "play-scale"
    )

    Card(
        Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(30.dp),
        colors = CardDefaults.cardColors(containerColor = LaunchSurface)
    ) {
        BoxWithConstraints(
            Modifier.fillMaxWidth().heightIn(min = 430.dp, max = 560.dp)
        ) {
            Surface(
                Modifier.size(260.dp).align(Alignment.TopEnd),
                shape = CircleShape,
                color = LaunchAccent.copy(alpha = 0.035f)
            ) {}
            Surface(
                Modifier.size(160.dp).align(Alignment.BottomEnd).padding(22.dp),
                shape = CircleShape,
                color = LaunchAccentStrong.copy(alpha = 0.055f)
            ) {}

            Column(
                Modifier.fillMaxSize().padding(30.dp),
                verticalArrangement = Arrangement.SpaceBetween
            ) {
                Column(verticalArrangement = Arrangement.spacedBy(9.dp)) {
                    Text("SELECTED INSTANCE", style = MaterialTheme.typography.labelSmall, color = LaunchAccent)
                    Text(
                        profile?.name ?: if (state is LauncherUiState.Loading) "Loading" else "Unavailable",
                        style = MaterialTheme.typography.headlineMedium,
                        fontWeight = FontWeight.SemiBold,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                    Text(
                        profile?.gameDir ?: "—",
                        style = MaterialTheme.typography.bodyMedium,
                        color = LaunchMuted,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                }

                Column(verticalArrangement = Arrangement.spacedBy(14.dp)) {
                    LaunchStateLine(state)
                    FilledTonalButton(
                        onClick = onPlay,
                        enabled = state is LauncherUiState.Ready,
                        modifier = Modifier.scale(playScale).fillMaxWidth().height(70.dp),
                        shape = RoundedCornerShape(21.dp)
                    ) {
                        Icon(Icons.Default.Launch, null)
                        Spacer(Modifier.width(10.dp))
                        Text(
                            when (state) {
                                is LauncherUiState.Launching -> "STARTING"
                                is LauncherUiState.Running -> "RUNNING"
                                else -> "PLAY"
                            },
                            style = MaterialTheme.typography.titleMedium,
                            fontWeight = FontWeight.SemiBold
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun LaunchStateLine(state: LauncherUiState) {
    val (primary, secondary) = when (state) {
        LauncherUiState.Loading -> "LOADING" to "Reading launcher state"
        is LauncherUiState.Ready -> "READY" to "Launch boundary is available"
        is LauncherUiState.Launching -> "STARTING" to "Launching the configured game"
        is LauncherUiState.Running -> "RUNNING" to "PID " + (state.pid ?: "unknown")
        is LauncherUiState.Error -> "ERROR" to state.message
    }

    Row(
        Modifier.fillMaxWidth()
            .border(1.dp, LaunchOutline, RoundedCornerShape(16.dp))
            .padding(horizontal = 15.dp, vertical = 13.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Surface(
            Modifier.size(9.dp),
            CircleShape,
            color = if (state is LauncherUiState.Error) Color(0xFFE57A7A) else LaunchAccent
        ) {}
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            Text(primary, style = MaterialTheme.typography.labelMedium, fontWeight = FontWeight.Bold)
            Text(
                secondary,
                style = MaterialTheme.typography.bodySmall,
                color = LaunchMuted,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis
            )
        }
    }
}

@Composable
private fun LatestReleaseCard() {
    val release = ImuxRelease.history.first()
    Card(
        Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(30.dp),
        colors = CardDefaults.cardColors(containerColor = LaunchSurface2)
    ) {
        Column(Modifier.padding(26.dp), verticalArrangement = Arrangement.spacedBy(15.dp)) {
            Text("WHAT'S NEW", style = MaterialTheme.typography.labelSmall, color = LaunchAccent)
            Text("v" + release.version, style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.SemiBold)
            Text(release.date, color = LaunchMuted, style = MaterialTheme.typography.bodySmall)
            Divider()
            release.entries.take(3).forEach { entry ->
                Row(verticalAlignment = Alignment.Top, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    Text("•", color = LaunchAccent, fontWeight = FontWeight.Bold)
                    Text(entry, color = LaunchMuted)
                }
            }
        }
    }
}

@Composable
private fun SoonRail() {
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(9.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("NEXT", style = MaterialTheme.typography.labelSmall, color = LaunchMuted)
            Spacer(Modifier.width(10.dp))
            Divider(Modifier.weight(1f))
        }
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            if (maxWidth < 760.dp) {
                Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    repeat(2) { SoonCard() }
                }
            } else {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    repeat(3) { Box(Modifier.weight(1f)) { SoonCard() } }
                }
            }
        }
    }
}

@Composable
private fun SoonCard() {
    Surface(
        Modifier.fillMaxWidth().height(70.dp),
        shape = RoundedCornerShape(18.dp),
        color = LaunchSurface,
        border = BorderStroke(1.dp, LaunchOutline)
    ) {
        Box(contentAlignment = Alignment.Center) {
            Text("SOON", style = MaterialTheme.typography.labelSmall, color = LaunchMuted)
        }
    }
}

@Composable
private fun LibraryPage(state: LauncherUiState) {
    val profile: InstallationProfile? = when (state) {
        is LauncherUiState.Ready -> state.profile
        is LauncherUiState.Launching -> state.profile
        is LauncherUiState.Running -> state.profile
        else -> null
    }

    PageFrame {
        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text("Library", style = MaterialTheme.typography.displaySmall, fontWeight = FontWeight.SemiBold)
            Text("The current launch target is separated from the renderer.", color = LaunchMuted)
        }

        Card(
            Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(26.dp),
            colors = CardDefaults.cardColors(containerColor = LaunchSurface)
        ) {
            Column(Modifier.padding(24.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                Text("ACTIVE", style = MaterialTheme.typography.labelSmall, color = LaunchAccent)
                Text(profile?.name ?: "Unavailable", style = MaterialTheme.typography.headlineSmall)
                DetailRow("Version", profile?.version ?: "—")
                DetailRow("Directory", profile?.gameDir ?: "—")
                DetailRow("State", profile?.status?.name ?: "—")
            }
        }

        SoonCard()
        SoonCard()
    }
}

@Composable
private fun ChangelogPage() {
    PageFrame {
        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text("Changelog", style = MaterialTheme.typography.displaySmall, fontWeight = FontWeight.SemiBold)
            Text("Release notes remain here until a new version is approved.", color = LaunchMuted)
        }

        ImuxRelease.history.forEach { release ->
            Card(
                Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(26.dp),
                colors = CardDefaults.cardColors(containerColor = LaunchSurface)
            ) {
                Column(Modifier.padding(24.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                    Text("VERSION " + release.version, style = MaterialTheme.typography.labelSmall, color = LaunchAccent)
                    Text(release.date, color = LaunchMuted, style = MaterialTheme.typography.bodySmall)
                    Divider()
                    release.entries.forEach { entry ->
                        Row(
                            horizontalArrangement = Arrangement.spacedBy(10.dp),
                            verticalAlignment = Alignment.Top
                        ) {
                            Text("•", color = LaunchAccent)
                            Text(entry, color = LaunchText)
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun SettingsPage() {
    PageFrame {
        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text("Settings", style = MaterialTheme.typography.displaySmall, fontWeight = FontWeight.SemiBold)
            Text("Only diagnostics have a concrete implementation right now.", color = LaunchMuted)
        }

        Card(
            Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(26.dp),
            colors = CardDefaults.cardColors(containerColor = LaunchSurface)
        ) {
            Column(Modifier.padding(24.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                DetailRow("Launcher version", ImuxRelease.CURRENT_VERSION)
                DetailRow("Config", ImuxPaths.config.toString())
                DetailRow("Logs", ImuxPaths.logs.toString())
                DetailRow("Instances", ImuxPaths.instances.toString())
            }
        }

        SoonCard()
        SoonCard()
    }
}

@Composable
private fun DetailRow(label: String, value: String) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(label, Modifier.weight(0.7f), color = LaunchMuted)
        Text(
            value,
            Modifier.weight(1.3f),
            maxLines = 2,
            overflow = TextOverflow.Ellipsis,
            fontWeight = FontWeight.Medium
        )
    }
}
