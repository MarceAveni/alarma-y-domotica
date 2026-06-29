package com.example.alarmaydomotica.data

import com.example.alarmaydomotica.data.model.FrenteState
import com.example.alarmaydomotica.data.model.PatioState
import kotlinx.coroutines.flow.StateFlow

interface HomeRepository {
    val patioState: StateFlow<PatioState>
    val frenteState: StateFlow<FrenteState>
    val isMqttConnected: StateFlow<Boolean>
    val isLocalMode: StateFlow<Boolean>
    val movementLogs: StateFlow<List<String>>

    fun setLocalMode(enabled: Boolean)
    fun setMqttConfig(host: String, port: Int, user: String, pass: String)
    fun setLocalIps(frenteIp: String, patioIp: String)
    
    // Configuración y envío de comandos
    fun sendPatioCommand(key: String, value: Any)
    fun sendPatioCommands(commands: Map<String, Any>)
    fun sendFrenteCommand(topic: String, value: Int)
    
    fun triggerPanic()
    fun clearLogs()
}
