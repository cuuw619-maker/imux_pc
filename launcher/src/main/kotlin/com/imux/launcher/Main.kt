package com.imux.launcher
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.WindowPlacement
import androidx.compose.ui.window.WindowState
import androidx.compose.ui.window.application
import com.imux.runtime.ImuxPaths

fun main() {
    ImuxPaths.initialize()
    application {
        Window(
            onCloseRequest = ::exitApplication,
            title = "Imux",
            icon = painterResource("icon.webp"),
            state = WindowState(placement = WindowPlacement.Maximized)
        ) {
            LauncherApp()
        }
    }
}
