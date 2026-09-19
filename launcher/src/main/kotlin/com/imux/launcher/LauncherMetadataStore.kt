package com.imux.launcher

import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.StandardOpenOption
import java.sql.DriverManager

class LauncherMetadataStore {
    private val root: Path = Path.of(
        System.getenv("APPDATA") ?: System.getProperty("user.home"),
        "Imux"
    )
    private val database: Path = root.resolve("launcher.db")

    fun initialize() {
        Files.createDirectories(root)
        DriverManager.getConnection("jdbc:sqlite:$database").use { connection ->
            connection.createStatement().use { statement ->
                statement.executeUpdate(
                    """
                    CREATE TABLE IF NOT EXISTS launcher_meta (
                        key TEXT PRIMARY KEY,
                        value TEXT NOT NULL
                    )
                    """.trimIndent()
                )
                statement.executeUpdate(
                    """
                    INSERT INTO launcher_meta(key, value)
                    VALUES ('schema_version', '1')
                    ON CONFLICT(key) DO NOTHING
                    """.trimIndent()
                )
            }
        }
    }

    fun databasePath(): Path = database
}
