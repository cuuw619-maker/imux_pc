package com.imux.modding.api
interface ModLoader { fun discover(): List<ModMetadata> }
data class ModMetadata(val id: String, val name: String, val version: String, val apiVersion: String, val entrypoint: String? = null)
interface ModContext { val modId: String; fun register(service: Any) }
interface ModAPI { val version: String }
interface ModRuntime { fun start(context: ModContext); fun stop() }