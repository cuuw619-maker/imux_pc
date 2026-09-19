package com.imux.launcher

import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.imux.core.model.InstallationProfile
import com.imux.core.service.GameLaunchService
import com.imux.runtime.GameLaunchServiceImpl
import kotlinx.coroutines.launch

private val Background = Color(0xFF070B10)
private val BackgroundDeep = Color(0xFF0B1118)
private val Accent = Color(0xFF7CF2BE)
private val AccentDark = Color(0xFF173F30)
private val TextPrimary = Color(0xFFEAF3EF)

@Composable
fun LauncherApp() {
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = Accent,
            onPrimary = Color(0xFF06150E),
            background = Background,
            surface = BackgroundDeep,
            onSurface = TextPrimary
        )
    ) {
        val gameLauncher: GameLaunchService = remember { GameLaunchServiceImpl() }
        val scope = rememberCoroutineScope()
        val profile = remember {
            InstallationProfile(
                id = "default",
                name = "Imux",
                version = "dev",
                gameDir = "instances/default"
            )
        }

        var launching by remember { mutableStateOf(false) }

        Box(Modifier.fillMaxSize()) {
            AnimatedBackdrop()

            Button(
                onClick = {
                    if (launching) return@Button
                    scope.launch {
                        launching = true
                        runCatching {
                            gameLauncher.launch(profile)
                        }
                        launching = false
                    }
                },
                enabled = !launching,
                modifier = Modifier
                    .width(300.dp)
                    .height(72.dp)
                    .align(Alignment.Center)
                    .shadow(
                        elevation = if (launching) 4.dp else 16.dp,
                        shape = RoundedCornerShape(22.dp),
                        ambientColor = Accent.copy(alpha = 0.22f),
                        spotColor = Accent.copy(alpha = 0.28f)
                    ),
                shape = RoundedCornerShape(22.dp),
                colors = ButtonDefaults.buttonColors(
                    containerColor = Accent,
                    contentColor = Color(0xFF06150E),
                    disabledContainerColor = Accent.copy(alpha = 0.42f),
                    disabledContentColor = Color(0xFF0C1A14)
                )
            ) {
                if (launching) {
                    CircularProgressIndicator(
                        modifier = Modifier.width(22.dp).height(22.dp),
                        strokeWidth = 2.5.dp,
                        color = Color(0xFF06150E)
                    )
                } else {
                    Icon(Icons.Default.PlayArrow, contentDescription = null)
                    Text("ИГРАТЬ", style = MaterialTheme.typography.titleMedium)
                }
            }
        }
    }
}

@Composable
private fun BoxScope.AnimatedBackdrop() {
    val transition = rememberInfiniteTransition(label = "launcher-background")
    val drift by transition.animateFloat(
        initialValue = 0f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(
            animation = tween(14000, easing = FastOutSlowInEasing),
            repeatMode = RepeatMode.Reverse
        ),
        label = "drift"
    )
    val pulse by transition.animateFloat(
        initialValue = 0.72f,
        targetValue = 1.0f,
        animationSpec = infiniteRepeatable(
            animation = tween(5200, easing = FastOutSlowInEasing),
            repeatMode = RepeatMode.Reverse
        ),
        label = "pulse"
    )

    Canvas(Modifier.fillMaxSize()) {
        drawRect(
            Brush.linearGradient(
                colors = listOf(BackgroundDeep, Background, Color(0xFF061015)),
                start = Offset(0f, size.height * drift),
                end = Offset(size.width, size.height * (1f - drift))
            )
        )

        val glowCenter = Offset(
            size.width * (0.18f + drift * 0.64f),
            size.height * (0.20f + (1f - drift) * 0.54f)
        )
        drawCircle(
            brush = Brush.radialGradient(
                colors = listOf(
                    Accent.copy(alpha = 0.12f * pulse),
                    Accent.copy(alpha = 0.035f * pulse),
                    Color.Transparent
                )
            ),
            radius = size.minDimension * 0.62f,
            center = glowCenter
        )

        val nodes = 20
        for (i in 0 until nodes) {
            val phase = i * 0.71f
            val x = size.width * (0.08f + ((i * 0.173f + drift * 0.21f) % 0.84f))
            val y = size.height * (0.08f + ((i * 0.317f + drift * 0.13f) % 0.84f))
            val radius = 1.5f + (kotlin.math.sin(phase + drift * 6.28f) + 1f) * 1.2f
            drawCircle(
                color = Accent.copy(alpha = 0.045f),
                radius = radius * 8f,
                center = Offset(x, y)
            )
            drawCircle(
                color = Accent.copy(alpha = 0.16f),
                radius = radius,
                center = Offset(x, y)
            )
        }

        for (i in 0 until 6) {
            val y = size.height * (0.13f + i * 0.15f) +
                kotlin.math.sin(drift * 6.28f + i) * size.height * 0.03f
            drawLine(
                color = Accent.copy(alpha = 0.035f),
                start = Offset(-size.width * 0.04f, y),
                end = Offset(size.width * 1.04f, y + size.height * 0.08f),
                strokeWidth = 1.5f
            )
        }
    }
}
