package com.imux.launcher
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application
import androidx.compose.ui.res.painterResource
import com.imux.runtime.ImuxPaths
fun main() {
    ImuxPaths.initialize()
    application {
        Window(onCloseRequest = ::exitApplication, title = "Imux", icon = painterResource("icon.webp")) { LauncherApp() }
    }
}