plugins {
    kotlin("jvm")
    kotlin("plugin.serialization")
}
dependencies {
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.8.1")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.10.2")
    testImplementation(kotlin("test"))
}
kotlin { jvmToolchain(21) }
