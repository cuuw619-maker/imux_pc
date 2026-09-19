package com.imux.runtime
import kotlin.test.Test
import kotlin.test.assertEquals
class PathTest { @Test fun rootDirectoryIsImux() { assertEquals("Imux", ImuxPaths.root.fileName.toString()) } }