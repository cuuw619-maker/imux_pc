package com.imux.launcher
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application
import androidx.compose.ui.res.painterResource
import com.imux.runtime.ImuxPaths
fun main() {
    ImuxPaths.initialize()
    application {
        Window(\n            onCloseRequest = ::exitApplication,\n            title = "Imux",\n            icon = painterResource("icon.webp"),\n            state = WindowState(placement = WindowPlacement.Maximized)\n        ) {\n            LauncherApp()\n        }
    }
}