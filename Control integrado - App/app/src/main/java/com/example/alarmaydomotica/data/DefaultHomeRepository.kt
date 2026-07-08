package com.example.alarmaydomotica.data

import android.util.Log
import com.example.alarmaydomotica.data.model.FrenteState
import com.example.alarmaydomotica.data.model.PatioState
import com.example.alarmaydomotica.data.model.ESP01State
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.serialization.json.*
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.eclipse.paho.client.mqttv3.*
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence
import java.text.SimpleDateFormat
import java.util.*
import java.util.concurrent.TimeUnit

class DefaultHomeRepository(
    private val externalScope: CoroutineScope = CoroutineScope(Dispatchers.Default + SupervisorJob())
) : HomeRepository {

    private val TAG = "DefaultHomeRepository"

    private val _patioState = MutableStateFlow(PatioState())
    override val patioState: StateFlow<PatioState> = _patioState.asStateFlow()

    private val _frenteState = MutableStateFlow(FrenteState())
    override val frenteState: StateFlow<FrenteState> = _frenteState.asStateFlow()

    private val _esp01State = MutableStateFlow(ESP01State())
    override val esp01State: StateFlow<ESP01State> = _esp01State.asStateFlow()

    private val _isMqttConnected = MutableStateFlow(false)
    override val isMqttConnected: StateFlow<Boolean> = _isMqttConnected.asStateFlow()

    private val _isLocalMode = MutableStateFlow(false)
    override val isLocalMode: StateFlow<Boolean> = _isLocalMode.asStateFlow()

    private val _movementLogs = MutableStateFlow<List<String>>(emptyList())
    override val movementLogs: StateFlow<List<String>> = _movementLogs.asStateFlow()

    // Configuración de red
    private var mqttHost = "broker.emqx.io"
    private var mqttPort = 1883
    private var mqttUser = "Marce"
    private var mqttPass = "Aveni793"

    private var frenteIp = "control_frente.local"
    private var patioIp = "control_patio.local"
    private var esp01Ip = "control_esp01.local"

    private var mqttClient: MqttClient? = null
    private val okHttpClient = OkHttpClient.Builder()
        .connectTimeout(3, TimeUnit.SECONDS)
        .readTimeout(3, TimeUnit.SECONDS)
        .build()

    private val json = Json { ignoreUnknownKeys = true }
    private val jsonMediaType = "application/json; charset=utf-8".toMediaType()

    private var connectionJob: Job? = null
    private var localPollingJob: Job? = null

    init {
        // Carga inicial y arranque de conexion
        startMqttConnection()
        startLocalPolling()
    }

    override fun setLocalMode(enabled: Boolean) {
        _isLocalMode.value = enabled
        if (enabled) {
            disconnectMqtt()
        } else {
            startMqttConnection()
        }
    }

    override fun setMqttConfig(host: String, port: Int, user: String, pass: String) {
        mqttHost = host
        mqttPort = port
        mqttUser = user
        mqttPass = pass
        if (!_isLocalMode.value) {
            startMqttConnection()
        }
    }

    override fun setLocalIps(frenteIp: String, patioIp: String, esp01Ip: String) {
        this.frenteIp = frenteIp
        this.patioIp = patioIp
        this.esp01Ip = esp01Ip
    }

    private fun startMqttConnection() {
        connectionJob?.cancel()
        connectionJob = externalScope.launch {
            while (isActive && !_isLocalMode.value) {
                try {
                    _isMqttConnected.value = false
                    val serverUri = "tcp://$mqttHost:$mqttPort"
                    val clientId = "androidClient_" + UUID.randomUUID().toString().substring(0, 5)
                    
                    mqttClient?.close()
                    val client = MqttClient(serverUri, clientId, MemoryPersistence())
                    mqttClient = client

                    val options = MqttConnectOptions().apply {
                        isCleanSession = true
                        connectionTimeout = 10
                        keepAliveInterval = 20
                        userName = mqttUser
                        password = mqttPass.toCharArray()
                        isAutomaticReconnect = true
                    }

                    client.setCallback(object : MqttCallbackExtended {
                        override fun connectComplete(reconnect: Boolean, serverURI: String?) {
                            Log.d(TAG, "Conexión MQTT completada. Reintento: $reconnect")
                            _isMqttConnected.value = true
                            subscribeToTopics(client)
                        }

                        override fun connectionLost(cause: Throwable?) {
                            Log.w(TAG, "Conexión MQTT perdida", cause)
                            _isMqttConnected.value = false
                        }

                        override fun messageArrived(topic: String?, message: MqttMessage?) {
                            if (topic != null && message != null) {
                                handleIncomingMqttMessage(topic, String(message.payload))
                            }
                        }

                        override fun deliveryComplete(token: IMqttDeliveryToken?) {}
                    })

                    Log.d(TAG, "Intentando conectar a MQTT $serverUri...")
                    client.connect(options)
                    break // Rompe el bucle si conecta (la reconexión auto se encarga de caídas temporales)
                } catch (e: Exception) {
                    Log.e(TAG, "Error al conectar MQTT, reintentando en 5s...", e)
                    delay(5000)
                }
            }
        }
    }

    private fun disconnectMqtt() {
        externalScope.launch {
            try {
                mqttClient?.disconnect()
                _isMqttConnected.value = false
                Log.d(TAG, "Desconectado de MQTT")
            } catch (e: Exception) {
                Log.e(TAG, "Error al desconectar MQTT", e)
            }
        }
    }

    private fun subscribeToTopics(client: MqttClient) {
        try {
            // Suscribirse a telemetría de Patio, Frente y ESP01
            client.subscribe("BALH142N1788/Patio", 1)
            client.subscribe("BALH142N1788/Frente", 1)
            client.subscribe("BALH142N1788/ESP01", 1)
            Log.d(TAG, "Suscrito a los temas MQTT del Frente, Patio y ESP01.")
        } catch (e: Exception) {
            Log.e(TAG, "Error al suscribirse", e)
        }
    }

    private fun handleIncomingMqttMessage(topic: String, payload: String) {
        try {
            when (topic) {
                "BALH142N1788/Patio" -> {
                    val parsed = json.decodeFromString<PatioState>(payload)
                    _patioState.value = parsed
                }
                "BALH142N1788/Frente" -> {
                    parseFrenteMqttJson(payload)
                }
                "BALH142N1788/ESP01" -> {
                    parseESP01MqttJson(payload)
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error parseando mensaje MQTT: Topic=$topic, Payload=$payload", e)
        }
    }

    private fun parseESP01MqttJson(payload: String) {
        try {
            val jsonDoc = json.parseToJsonElement(payload).jsonObject
            val c1ST = jsonDoc["canal1ST"]?.jsonPrimitive?.booleanOrNull ?: false
            val c1Conf = jsonDoc["canal1Conf"]?.jsonPrimitive?.intOrNull ?: 0
            val c2ST = jsonDoc["canal2ST"]?.jsonPrimitive?.booleanOrNull ?: false
            val c2Conf = jsonDoc["canal2Conf"]?.jsonPrimitive?.intOrNull ?: 0
            val luz = jsonDoc["luzAmbiente"]?.jsonPrimitive?.content ?: "dia"
            val interval = jsonDoc["intervalData"]?.jsonPrimitive?.intOrNull ?: 60

            val schedulesJson = jsonDoc["schedules"]?.jsonArray
            val schedulesList = schedulesJson?.map { elem ->
                json.decodeFromJsonElement<com.example.alarmaydomotica.data.model.ScheduleEvent>(elem)
            } ?: emptyList()

            _esp01State.value = ESP01State(
                canal1ST = c1ST,
                canal1Conf = c1Conf,
                canal2ST = c2ST,
                canal2Conf = c2Conf,
                luzAmbiente = luz,
                intervalData = interval,
                schedules = schedulesList
            )
        } catch (e: Exception) {
            Log.e(TAG, "Error parseando MQTT ESP01: $payload", e)
        }
    }

    private fun parseESP01HttpJson(payload: String) {
        try {
            val jsonDoc = json.parseToJsonElement(payload).jsonObject
            val c1ST = jsonDoc["canal1_st"]?.jsonPrimitive?.booleanOrNull ?: false
            val c1Conf = jsonDoc["canal1_conf"]?.jsonPrimitive?.intOrNull ?: 0
            val c2ST = jsonDoc["canal2_st"]?.jsonPrimitive?.booleanOrNull ?: false
            val c2Conf = jsonDoc["canal2_conf"]?.jsonPrimitive?.intOrNull ?: 0
            val luz = jsonDoc["luz_ambiente"]?.jsonPrimitive?.content ?: "dia"
            val interval = jsonDoc["interval_data"]?.jsonPrimitive?.intOrNull ?: 60

            val schedulesJson = jsonDoc["schedules"]?.jsonArray
            val schedulesList = schedulesJson?.map { elem ->
                json.decodeFromJsonElement<com.example.alarmaydomotica.data.model.ScheduleEvent>(elem)
            } ?: emptyList()

            _esp01State.value = ESP01State(
                canal1ST = c1ST,
                canal1Conf = c1Conf,
                canal2ST = c2ST,
                canal2Conf = c2Conf,
                luzAmbiente = luz,
                intervalData = interval,
                schedules = schedulesList
            )
        } catch (e: Exception) {
            Log.e(TAG, "Error parseando HTTP ESP01: $payload", e)
        }
    }

    override fun sendESP01Command(key: String, value: Any) {
        val isMqttMode = _isMqttConnected.value && !_isLocalMode.value
        externalScope.launch(Dispatchers.IO) {
            try {
                if (isMqttMode) {
                    val mqttKey = when (key) {
                        "canal1_conf", "canal1Conf" -> "canal1Conf"
                        "canal2_conf", "canal2Conf" -> "canal2Conf"
                        "interval_data", "intervalData" -> "intervalData"
                        else -> key
                    }
                    val jsonPayload = Json.encodeToString(JsonObject.serializer(), buildJsonObject(mapOf(mqttKey to value)))
                    val message = MqttMessage(jsonPayload.toByteArray()).apply { qos = 1 }
                    mqttClient?.publish("BALH142N1788/Aveni793", message)
                    Log.d(TAG, "Comando ESP01 MQTT exitoso: $jsonPayload")
                } else {
                    val httpKey = when (key) {
                        "canal1Conf", "canal1_conf" -> "canal1_conf"
                        "canal2Conf", "canal2_conf" -> "canal2_conf"
                        "intervalData", "interval_data" -> "interval_data"
                        else -> key
                    }
                    val jsonPayload = Json.encodeToString(JsonObject.serializer(), buildJsonObject(mapOf(httpKey to value)))
                    val body = jsonPayload.toRequestBody(jsonMediaType)
                    val request = Request.Builder()
                        .url("http://$esp01Ip/api/config")
                        .post(body)
                        .build()
                    okHttpClient.newCall(request).execute().use { response ->
                        if (response.isSuccessful) {
                            Log.d(TAG, "Comando ESP01 HTTP local exitoso: $jsonPayload")
                            updateESP01LocalState(httpKey, value)
                        } else {
                            Log.e(TAG, "Error HTTP ESP01 local: ${response.code}")
                        }
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Fallo al enviar comando ESP01: key=$key, val=$value", e)
            }
        }
    }

    private fun updateESP01LocalState(key: String, value: Any) {
        val intVal = (value as? Number)?.toInt() ?: 0
        when (key) {
            "canal1_conf", "canal1Conf" -> _esp01State.value = _esp01State.value.copy(canal1Conf = intVal)
            "canal2_conf", "canal2Conf" -> _esp01State.value = _esp01State.value.copy(canal2Conf = intVal)
            "interval_data", "intervalData" -> _esp01State.value = _esp01State.value.copy(intervalData = intVal)
        }
    }

    private fun parseFrenteMqttJson(payload: String) {

        try {
            val jsonDoc = json.parseToJsonElement(payload).jsonObject
            val temp = jsonDoc["Temperatura"]?.jsonPrimitive?.intOrNull ?: 0
            val hum = jsonDoc["Humedad"]?.jsonPrimitive?.intOrNull ?: 0
            val luzStr = jsonDoc["Luz"]?.jsonPrimitive?.content ?: "noche"
            val sensorTrigger = jsonDoc["Sensor"]?.jsonPrimitive?.intOrNull ?: 0
            val movimientos = jsonDoc["Movimientos"]?.jsonPrimitive?.intOrNull ?: 0
            val camaras = jsonDoc["Camaras"]?.jsonPrimitive?.booleanOrNull ?: true
            val pir1 = jsonDoc["PIR1En"]?.jsonPrimitive?.booleanOrNull ?: true
            val pir2 = jsonDoc["PIR2En"]?.jsonPrimitive?.booleanOrNull ?: true
            val sirConf = jsonDoc["SirConf"]?.jsonPrimitive?.intOrNull ?: 0
            val refConf = jsonDoc["RefConf"]?.jsonPrimitive?.intOrNull ?: 0
            val luzVConf = jsonDoc["LuzVConf"]?.jsonPrimitive?.intOrNull ?: 0
            
            val schedulesJson = jsonDoc["schedules"]?.jsonArray
            val schedulesList = schedulesJson?.map { elem ->
                json.decodeFromJsonElement<com.example.alarmaydomotica.data.model.ScheduleEvent>(elem)
            } ?: emptyList()

            val oldState = _frenteState.value
            val pir1Triggered = sensorTrigger == 1
            val pir2Triggered = sensorTrigger == 2

            if (pir1Triggered && !oldState.pir1Mov) {
                addMovementLog("PIR 1 Frente Detectó Movimiento")
            }
            if (pir2Triggered && !oldState.pir2Mov) {
                addMovementLog("PIR 2 Lateral Detectó Movimiento")
            }

            _frenteState.value = FrenteState(
                temp = temp,
                hum = hum,
                luz = if (luzStr.contains("día", ignoreCase = true)) "día" else "noche",
                pir1Mov = pir1Triggered,
                pir2Mov = pir2Triggered,
                countMov = movimientos,
                reflectoresConf = refConf,
                luzVeredaConf = luzVConf,
                sirenaConf = sirConf,
                camarasEn = camaras,
                pir1En = pir1,
                pir2En = pir2,
                schedules = schedulesList
            )
        } catch (e: Exception) {
            Log.e(TAG, "Error parseando JSON MQTT del Frente", e)
        }
    }

    private fun parseFrenteHttpJson(payload: String) {
        try {
            val jsonDoc = json.parseToJsonElement(payload).jsonObject
            val temp = jsonDoc["temp"]?.jsonPrimitive?.intOrNull ?: 0
            val hum = jsonDoc["hum"]?.jsonPrimitive?.intOrNull ?: 0
            val luzTxt = jsonDoc["luz_txt"]?.jsonPrimitive?.content ?: "Noche"
            val sensorTrigger = jsonDoc["sensor_trigger"]?.jsonPrimitive?.intOrNull ?: 0
            val movimientos = jsonDoc["mov_count"]?.jsonPrimitive?.intOrNull ?: 0
            val camaras = jsonDoc["camaras_en"]?.jsonPrimitive?.booleanOrNull ?: true
            val pir1 = jsonDoc["pir1_en"]?.jsonPrimitive?.booleanOrNull ?: true
            val pir2 = jsonDoc["pir2_en"]?.jsonPrimitive?.booleanOrNull ?: true
            val sirConf = jsonDoc["sirena_conf"]?.jsonPrimitive?.intOrNull ?: 0
            val refConf = jsonDoc["reflectores_conf"]?.jsonPrimitive?.intOrNull ?: 0
            val luzVConf = jsonDoc["luz_vereda_conf"]?.jsonPrimitive?.intOrNull ?: 0

            val schedulesJson = jsonDoc["schedules"]?.jsonArray
            val schedulesList = schedulesJson?.map { elem ->
                json.decodeFromJsonElement<com.example.alarmaydomotica.data.model.ScheduleEvent>(elem)
            } ?: emptyList()

            val oldState = _frenteState.value
            val pir1Triggered = sensorTrigger == 1
            val pir2Triggered = sensorTrigger == 2

            if (pir1Triggered && !oldState.pir1Mov) {
                addMovementLog("PIR 1 Frente Detectó Movimiento (HTTP)")
            }
            if (pir2Triggered && !oldState.pir2Mov) {
                addMovementLog("PIR 2 Lateral Detectó Movimiento (HTTP)")
            }

            _frenteState.value = FrenteState(
                temp = temp,
                hum = hum,
                luz = if (luzTxt.equals("Dia", ignoreCase = true)) "día" else "noche",
                pir1Mov = pir1Triggered,
                pir2Mov = pir2Triggered,
                countMov = movimientos,
                reflectoresConf = refConf,
                luzVeredaConf = luzVConf,
                sirenaConf = sirConf,
                camarasEn = camaras,
                pir1En = pir1,
                pir2En = pir2,
                schedules = schedulesList
            )
        } catch (e: Exception) {
            Log.e(TAG, "Error parseando JSON HTTP del Frente", e)
        }
    }

    private fun extractNumber(text: String): Int? {
        val matches = Regex("\\d+").findAll(text)
        return matches.lastOrNull()?.value?.toIntOrNull()
    }

    private fun addMovementLog(message: String) {
        val sdf = SimpleDateFormat("dd/MM HH:mm:ss", Locale.getDefault())
        val formattedDate = sdf.format(Date())
        val newLog = "[$formattedDate] $message"
        _movementLogs.value = listOf(newLog) + _movementLogs.value.take(49) // Limite de 50 logs
    }

    // ==========================================
    // ENVÍO DE COMANDOS (MQTT / LOCAL HTTP FALLBACK)
    // ==========================================

    override fun sendPatioCommand(key: String, value: Any) {
        sendPatioCommands(mapOf(key to value))
    }

    override fun sendPatioCommands(commands: Map<String, Any>) {
        val jsonPayload = Json.encodeToString(JsonObject.serializer(), buildJsonObject(commands))
        
        if (_isMqttConnected.value && !_isLocalMode.value) {
            // Remoto vía MQTT
            externalScope.launch {
                try {
                    val message = MqttMessage(jsonPayload.toByteArray()).apply { qos = 1 }
                    mqttClient?.publish("BALH142N1788/Aveni793", message)
                    Log.d(TAG, "Comando Patio enviado vía MQTT: $jsonPayload")
                } catch (e: Exception) {
                    Log.e(TAG, "Error enviando comando Patio vía MQTT, intentando local...", e)
                    sendPatioLocalHttp(jsonPayload)
                }
            }
        } else {
            // Local directo vía HTTP
            sendPatioLocalHttp(jsonPayload)
        }
    }

    private fun buildJsonObject(map: Map<String, Any>): JsonObject {
        val content = map.mapValues { (_, value) ->
            when (value) {
                is Number -> JsonPrimitive(value)
                is Boolean -> JsonPrimitive(value)
                else -> JsonPrimitive(value.toString())
            }
        }
        return JsonObject(content)
    }

    override fun sendFrenteCommand(topic: String, value: Int) {
        val isMqttMode = _isMqttConnected.value && !_isLocalMode.value
        
        externalScope.launch(Dispatchers.IO) {
            try {
                if (isMqttMode) {
                    val key = when (topic) {
                        "Iluminacion/Reflectores" -> "ReflectoresConf"
                        "Iluminacion/Vereda" -> "LuzVeredaConf"
                        "Alarma/Sirena" -> "SirenaConf"
                        "Alarma/Camaras" -> "CamarasEn"
                        "Alarma/PIR1En" -> "PIR1En"
                        "Alarma/PIR2En" -> "PIR2En"
                        else -> topic
                    }
                    val cmdVal: Any = when (key) {
                        "CamarasEn", "PIR1En", "PIR2En" -> value == 1
                        else -> value
                    }
                    val jsonPayload = Json.encodeToString(JsonObject.serializer(), buildJsonObject(mapOf(key to cmdVal)))
                    
                    val message = MqttMessage(jsonPayload.toByteArray()).apply { qos = 1 }
                    mqttClient?.publish("BALH142N1788/Aveni793", message)
                    Log.d(TAG, "Comando Frente enviado vía MQTT: $jsonPayload")
                } else {
                    val httpKey = when (topic) {
                        "Iluminacion/Reflectores" -> "reflectores_conf"
                        "Iluminacion/Vereda" -> "luz_vereda_conf"
                        "Alarma/Sirena" -> "sirena_conf"
                        "Alarma/Camaras" -> "camaras_en"
                        "Alarma/PIR1En" -> "pir1_en"
                        "Alarma/PIR2En" -> "pir2_en"
                        else -> topic
                    }
                    val cmdVal: Any = when (httpKey) {
                        "camaras_en", "pir1_en", "pir2_en" -> value == 1
                        else -> value
                    }
                    val jsonPayload = Json.encodeToString(JsonObject.serializer(), buildJsonObject(mapOf(httpKey to cmdVal)))
                    val body = jsonPayload.toRequestBody(jsonMediaType)
                    val request = Request.Builder()
                        .url("http://$frenteIp/api/config")
                        .post(body)
                        .build()
                    
                    okHttpClient.newCall(request).execute().use { response ->
                        if (response.isSuccessful) {
                            Log.d(TAG, "Comando Frente HTTP local exitoso: $jsonPayload")
                            updateFrenteLocalState(topic, value)
                        } else {
                            Log.e(TAG, "Error HTTP Frente local: ${response.code}")
                        }
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Fallo al enviar comando Frente: topic=$topic, val=$value", e)
            }
        }
    }

    private fun updateFrenteLocalState(topic: String, value: Int) {
        when (topic) {
            "Iluminacion/Reflectores" -> _frenteState.value = _frenteState.value.copy(reflectoresConf = value)
            "Iluminacion/Vereda" -> _frenteState.value = _frenteState.value.copy(luzVeredaConf = value)
            "Alarma/Sirena" -> _frenteState.value = _frenteState.value.copy(sirenaConf = value)
            "Alarma/Camaras" -> _frenteState.value = _frenteState.value.copy(camarasEn = value == 1)
            "Alarma/PIR1En" -> _frenteState.value = _frenteState.value.copy(pir1En = value == 1)
            "Alarma/PIR2En" -> _frenteState.value = _frenteState.value.copy(pir2En = value == 1)
        }
    }

    override fun triggerPanic() {
        val currentSirena = frenteState.value.sirenaConf
        val target = if (currentSirena == 0) 1 else 0
        // Activa la sirena tanto en Frente como en Patio
        sendFrenteCommand("Alarma/Sirena", target)
        sendPatioCommand("reflectoresConf", if (target == 1) 1 else 0) // Prende reflectores patio
    }

    override fun clearLogs() {
        _movementLogs.value = emptyList()
    }

    // ==========================================
    // LLAMADAS LOCALES HTTP (MODO LOCAL)
    // ==========================================

    private fun sendPatioLocalHttp(jsonPayload: String) {
        externalScope.launch(Dispatchers.IO) {
            try {
                val body = jsonPayload.toRequestBody(jsonMediaType)
                val request = Request.Builder()
                    .url("http://$patioIp/control")
                    .post(body)
                    .build()
                okHttpClient.newCall(request).execute().use { response ->
                    if (response.isSuccessful) {
                        Log.d(TAG, "Comando Patio HTTP exitoso")
                        // Refrescamos estado si responde con estado
                        val respBody = response.body?.string()
                        if (!respBody.isNullOrEmpty() && respBody.startsWith("{")) {
                            updatePatioFromJson(respBody)
                        }
                    } else {
                        Log.e(TAG, "Error HTTP Patio: ${response.code}")
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Fallo de conexión HTTP local a Patio ESP32", e)
            }
        }
    }

    private fun sendFrenteLocalHttp(topic: String, value: Int) {
        externalScope.launch(Dispatchers.IO) {
            try {
                // Endpoint local simplificado: GET http://<ip>/control?topic=X&val=Y
                val request = Request.Builder()
                    .url("http://$frenteIp/control?topic=$topic&val=$value")
                    .build()
                okHttpClient.newCall(request).execute().use { response ->
                    if (response.isSuccessful) {
                        Log.d(TAG, "Comando Frente HTTP exitoso")
                        // Actualizar localmente el estado de la variable enviada
                        updateFrenteLocalState(topic, value)
                    } else {
                        Log.e(TAG, "Error HTTP Frente: ${response.code}")
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Fallo de conexión HTTP local a Frente ESP8266", e)
            }
        }
    }

    private fun updatePatioFromJson(jsonStr: String) {
        try {
            val parsed = json.decodeFromString<PatioState>(jsonStr)
            _patioState.value = parsed
        } catch (e: Exception) {
            Log.e(TAG, "Error parseando JSON de respuesta local del Patio", e)
        }
    }

    // ==========================================
    // POLLING LOCAL DE TELEMETRÍA (PARA MODO LOCAL)
    // ==========================================

    private fun startLocalPolling() {
        localPollingJob?.cancel()
        localPollingJob = externalScope.launch(Dispatchers.IO) {
            while (isActive) {
                // Solo consultamos por HTTP periódicamente si no estamos conectados a MQTT
                // o si explícitamente activamos Modo Local
                if (!_isMqttConnected.value || _isLocalMode.value) {
                    pollPatioStatus()
                    pollFrenteStatus()
                    pollESP01Status()
                }
                delay(5000) // Poll cada 5 segundos
            }
        }
    }

    private fun pollESP01Status() {
        try {
            val request = Request.Builder()
                .url("http://$esp01Ip/api/status")
                .build()
            okHttpClient.newCall(request).execute().use { response ->
                if (response.isSuccessful) {
                    val bodyStr = response.body?.string() ?: ""
                    if (bodyStr.isNotEmpty() && bodyStr.startsWith("{")) {
                        parseESP01HttpJson(bodyStr)
                    }
                }
            }
        } catch (e: Exception) {
            Log.v(TAG, "Polling ESP01 local falló: ${e.message}")
        }
    }

    private fun pollPatioStatus() {
        try {
            val request = Request.Builder()
                .url("http://$patioIp/status")
                .build()
            okHttpClient.newCall(request).execute().use { response ->
                if (response.isSuccessful) {
                    val bodyStr = response.body?.string() ?: ""
                    if (bodyStr.isNotEmpty() && bodyStr.startsWith("{")) {
                        updatePatioFromJson(bodyStr)
                    }
                }
            }
        } catch (e: Exception) {
            // Silencioso para evitar saturación de logs si no está disponible
            Log.v(TAG, "Polling Patio local falló: ${e.message}")
        }
    }

    private fun pollFrenteStatus() {
        try {
            val request = Request.Builder()
                .url("http://$frenteIp/api/status")
                .build()
            okHttpClient.newCall(request).execute().use { response ->
                if (response.isSuccessful) {
                    val bodyStr = response.body?.string() ?: ""
                    if (bodyStr.isNotEmpty() && bodyStr.startsWith("{")) {
                        parseFrenteHttpJson(bodyStr)
                    }
                }
            }
        } catch (e: Exception) {
            Log.v(TAG, "Polling Frente local falló: ${e.message}")
        }
    }

    override fun sendSchedules(device: String, schedules: List<com.example.alarmaydomotica.data.model.ScheduleEvent>) {
        val isMqttMode = _isMqttConnected.value && !_isLocalMode.value
        
        externalScope.launch(Dispatchers.IO) {
            try {
                val schedulesJsonArray = json.encodeToJsonElement(schedules).jsonArray
                
                if (isMqttMode) {
                    val mqttKey = when (device) {
                        "Frente" -> "frenteSchedules"
                        "Patio" -> "patioSchedules"
                        "ESP01" -> "esp01Schedules"
                        else -> "schedules"
                    }
                    val jsonPayload = json.encodeToString(
                        JsonObject.serializer(),
                        buildJsonObject {
                            put(mqttKey, schedulesJsonArray)
                        }
                    )
                    val message = MqttMessage(jsonPayload.toByteArray()).apply { qos = 1 }
                    mqttClient?.publish("BALH142N1788/Aveni793", message)
                    Log.d(TAG, "Schedules MQTT enviados para $device: $jsonPayload")
                } else {
                    val jsonPayload = json.encodeToString(
                        JsonObject.serializer(),
                        buildJsonObject {
                            put("schedules", schedulesJsonArray)
                        }
                    )
                    val body = jsonPayload.toRequestBody(jsonMediaType)
                    val (ip, path) = when (device) {
                        "Frente" -> Pair(frenteIp, "/api/config")
                        "Patio" -> Pair(patioIp, "/control")
                        "ESP01" -> Pair(esp01Ip, "/api/config")
                        else -> Pair("", "")
                    }
                    if (ip.isNotEmpty()) {
                        val request = Request.Builder()
                            .url("http://$ip$path")
                            .post(body)
                            .build()
                        okHttpClient.newCall(request).execute().use { response ->
                            if (response.isSuccessful) {
                                Log.d(TAG, "Schedules HTTP local exitoso para $device")
                                when (device) {
                                    "Frente" -> _frenteState.value = _frenteState.value.copy(schedules = schedules)
                                    "Patio" -> _patioState.value = _patioState.value.copy(schedules = schedules)
                                    "ESP01" -> _esp01State.value = _esp01State.value.copy(schedules = schedules)
                                }
                            } else {
                                Log.e(TAG, "Error HTTP local Schedules para $device: ${response.code}")
                            }
                        }
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Fallo al enviar schedules para $device", e)
            }
        }
    }
}
object DefaultDataRepository {
    private val instance = DefaultHomeRepository()
    operator fun invoke(): HomeRepository = instance
}
