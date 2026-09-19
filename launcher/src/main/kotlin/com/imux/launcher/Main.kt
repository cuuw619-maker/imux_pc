package com.imux.launcher

import java.awt.BorderLayout
import java.awt.Color
import java.awt.Dimension
import java.awt.Font
import java.awt.GradientPaint
import java.awt.Graphics
import java.awt.Graphics2D
import java.awt.GridBagLayout
import java.awt.RenderingHints
import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.Paths
import javax.swing.BorderFactory
import javax.swing.JButton
import javax.swing.JFrame
import javax.swing.JPanel
import javax.swing.SwingUtilities

private const val GAME_EXE = "ImuxGame.exe"

fun main() {
    SwingUtilities.invokeLater {
        val frame = JFrame("Imux")
        frame.defaultCloseOperation = JFrame.EXIT_ON_CLOSE
        frame.minimumSize = Dimension(900, 600)
        frame.setSize(1280, 720)
        frame.setLocationRelativeTo(null)
        frame.contentPane = LauncherPanel()
        frame.isVisible = true
    }
}

private class LauncherPanel : JPanel(BorderLayout()) {
    private val playButton = JButton("ИГРАТЬ")

    init {
        isOpaque = false
        playButton.font = Font("Segoe UI", Font.BOLD, 20)
        playButton.preferredSize = Dimension(300, 72)
        playButton.background = Color(124, 242, 190)
        playButton.foreground = Color(8, 18, 14)
        playButton.focusPainted = false
        playButton.border = BorderFactory.createEmptyBorder()
        playButton.addActionListener { startGame() }

        val center = JPanel(GridBagLayout())
        center.isOpaque = false
        center.add(playButton)
        add(center, BorderLayout.CENTER)
    }

    override fun paintComponent(graphics: Graphics) {
        val g = graphics.create() as Graphics2D
        g.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_SPEED)
        g.paint = GradientPaint(
            0f, 0f, Color(7, 11, 16),
            width.toFloat(), height.toFloat(), Color(10, 24, 28)
        )
        g.fillRect(0, 0, width, height)
        g.color = Color(124, 242, 190, 24)
        val glow = minOf(width, height)
        g.fillOval(width / 2 - glow / 2, height / 2 - glow / 2, glow, glow)
        g.dispose()
        super.paintComponent(graphics)
    }

    private fun startGame() {
        val game = findGame() ?: return
        playButton.isEnabled = false
        runCatching {
            ProcessBuilder(game.toString())
                .directory(game.parent.toFile())
                .start()
        }.onFailure {
            playButton.isEnabled = true
        }
    }
}

private fun findGame(): Path? {
    val candidates = linkedSetOf<Path>()
    candidates += Paths.get(System.getProperty("user.dir"), GAME_EXE)

    runCatching {
        ProcessHandle.current().info().command().orElse(null)?.let {
            Paths.get(it).toAbsolutePath().normalize().parent?.resolve(GAME_EXE)?.let(candidates::add)
        }
    }

    runCatching {
        val code = Paths.get(
            LauncherPanel::class.java.protectionDomain.codeSource.location.toURI()
        ).toAbsolutePath().normalize()
        val root = if (Files.isDirectory(code)) code else code.parent
        root?.parent?.resolve(GAME_EXE)?.let(candidates::add)
    }

    candidates += Paths.get("build/native/bin", GAME_EXE).toAbsolutePath()
    return candidates.firstOrNull(Files::isRegularFile)
}
