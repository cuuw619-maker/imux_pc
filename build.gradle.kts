plugins {
    kotlin("jvm") version "2.1.21" apply false
}

group = "com.imux"
version = "0.0.1"

subprojects {
    group = rootProject.group
    version = rootProject.version

    tasks.withType<org.jetbrains.kotlin.gradle.tasks.KotlinCompile>().configureEach {
        compilerOptions.jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_21)
    }

    tasks.withType<JavaCompile>().configureEach {
        options.release.set(21)
    }
}
