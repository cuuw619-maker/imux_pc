plugins {
    kotlin("jvm")
    kotlin("plugin.compose")
    id("org.jetbrains.compose")
}
dependencies {
    implementation(project(":core"))
    implementation(project(":runtime"))
    implementation(compose.desktop.currentOs)
    implementation(compose.material3)
    implementation(compose.runtime)
    implementation(compose.foundation)
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.10.2")
    testImplementation(kotlin("test"))
}
kotlin { jvmToolchain(21) }
compose.desktop {
    application {
        mainClass = "com.imux.launcher.MainKt"
        nativeDistributions {
            targetFormats(org.jetbrains.compose.desktop.application.dsl.TargetFormat.Exe)
            packageName = "ImuxDev"
            packageVersion = "0.1.0"
            description = "Imux development launcher shell"
            vendor = "Imux"
            windows {
                menuGroup = "Imux"
                shortcut = false
                dirChooser = false
            }
        }
    }
}
