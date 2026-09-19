plugins { kotlin("jvm") }
dependencies { implementation(project(":core")); testImplementation(kotlin("test")) }
kotlin { jvmToolchain(21) }