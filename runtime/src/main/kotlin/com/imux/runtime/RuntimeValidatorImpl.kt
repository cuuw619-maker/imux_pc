package com.imux.runtime
import com.imux.core.model.RuntimeConfig
import com.imux.core.service.RuntimeValidation
import com.imux.core.service.RuntimeValidator
import java.nio.file.Files
import java.nio.file.Path
class RuntimeValidatorImpl : RuntimeValidator {
    override suspend fun validate(runtime: RuntimeConfig): RuntimeValidation {
        val configured = runtime.javaPath?.takeIf(String::isNotBlank)?.let(Path::of)
        if (configured != null && Files.isRegularFile(configured)) return RuntimeValidation(true, configured, "Configured Java runtime is available.")
        val home = System.getProperty("java.home")?.let(Path::of)
        val exe = if (System.getProperty("os.name").startsWith("Windows", true)) "java.exe" else "java"
        val discovered = home?.resolve("bin")?.resolve(exe)
        return if (discovered != null && Files.isRegularFile(discovered)) RuntimeValidation(true, discovered, "Current JVM runtime is available.")
        else RuntimeValidation(false, message = "Java runtime not found.")
    }
}