package com.example.alarmaydomotica.ui.screens

import android.widget.Toast
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.alarmaydomotica.R
import com.example.alarmaydomotica.data.HomeRepository
import com.example.alarmaydomotica.theme.*
import com.example.alarmaydomotica.data.model.PatioState

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConfiguracionScreen(
    repository: HomeRepository,
    onItemClick: ((androidx.navigation3.runtime.NavKey) -> Unit)? = null,
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    val patioState by repository.patioState.collectAsState()
    val isLocalMode by repository.isLocalMode.collectAsState()

    // --- Estados Locales para Inputs ---
    var isLocalModeState by remember { mutableStateOf(isLocalMode) }
    var hostMqtt by remember { mutableStateOf("broker.emqx.io") }
    var portMqtt by remember { mutableStateOf("1883") }
    var userMqtt by remember { mutableStateOf("Marce") }
    var passMqtt by remember { mutableStateOf("Aveni793") }

    var localIpFrente by remember { mutableStateOf("control_frente.local") }
    var localIpPatio by remember { mutableStateOf("control_patio.local") }
    var localIpESP01 by remember { mutableStateOf("control_esp01.local") }


    // Parámetros del Climatizador
    var diffOn by remember { mutableStateOf(patioState.tempDif_on.toString()) }
    var diffOff by remember { mutableStateOf(patioState.tempDif_off.toString()) }
    
    // Tiempos de Bomba
    var minOn by remember { mutableStateOf((patioState.minOnTime / 60000).toString()) }
    var minOff by remember { mutableStateOf((patioState.minOffTime / 60000).toString()) }
    var maxOn by remember { mutableStateOf((patioState.maxOnTime / 60000).toString()) }

    // Sensores
    var ldrNightVal by remember { mutableStateOf(patioState.ValorNoche.toString()) }
    var ldrNightHist by remember { mutableStateOf(patioState.histeresisLuz.toString()) }

    // Sincronizar estados cuando cambian los valores del hardware
    LaunchedEffect(patioState) {
        diffOn = patioState.tempDif_on.toString()
        diffOff = patioState.tempDif_off.toString()
        minOn = (patioState.minOnTime / 60000).toString()
        minOff = (patioState.minOffTime / 60000).toString()
        maxOn = (patioState.maxOnTime / 60000).toString()
        ldrNightVal = patioState.ValorNoche.toString()
        ldrNightHist = patioState.histeresisLuz.toString()
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // --- SECCIÓN CONEXIÓN ---
        SectionHeader(title = stringResource(R.string.network_settings), icon = Icons.Default.SettingsEthernet, tint = BlueNeon)

        // Modo Local Switch
        Card(
            modifier = Modifier.fillMaxWidth().border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
            colors = CardDefaults.cardColors(containerColor = DarkSurface)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(16.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Column {
                    Text(
                        text = "Forzar Modo Local",
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color.White
                    )
                    Text(
                        text = "Ignora MQTT, se comunica directo en la WiFi local",
                        fontSize = 11.sp,
                        color = OnSurfaceDark.copy(alpha = 0.5f)
                    )
                }
                Switch(
                    checked = isLocalModeState,
                    onCheckedChange = {
                        isLocalModeState = it
                        repository.setLocalMode(it)
                        Toast.makeText(context, "Modo local: ${if (it) "Activado" else "Desactivado"}", Toast.LENGTH_SHORT).show()
                    },
                    colors = SwitchDefaults.colors(
                        checkedThumbColor = BlueNeon,
                        checkedTrackColor = BlueNeon.copy(alpha = 0.3f)
                    )
                )
            }
        }

        // Formulario MQTT
        Card(
            modifier = Modifier.fillMaxWidth().border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
            colors = CardDefaults.cardColors(containerColor = DarkSurface)
        ) {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text(text = stringResource(R.string.mqtt_settings), fontSize = 13.sp, fontWeight = FontWeight.Bold, color = Color.White)
                
                OutlinedTextField(
                    value = hostMqtt,
                    onValueChange = { hostMqtt = it },
                    label = { Text("Broker Host") },
                    modifier = Modifier.fillMaxWidth(),
                    colors = textFieldColors()
                )
                OutlinedTextField(
                    value = portMqtt,
                    onValueChange = { portMqtt = it },
                    label = { Text("Puerto") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    modifier = Modifier.fillMaxWidth(),
                    colors = textFieldColors()
                )
                Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(
                        value = userMqtt,
                        onValueChange = { userMqtt = it },
                        label = { Text("Usuario") },
                        modifier = Modifier.weight(1f),
                        colors = textFieldColors()
                    )
                    OutlinedTextField(
                        value = passMqtt,
                        onValueChange = { passMqtt = it },
                        label = { Text("Contraseña") },
                        modifier = Modifier.weight(1f),
                        colors = textFieldColors()
                    )
                }

                Button(
                    onClick = {
                        val port = portMqtt.toIntOrNull() ?: 1883
                        repository.setMqttConfig(hostMqtt, port, userMqtt, passMqtt)
                        Toast.makeText(context, "Configuración MQTT Guardada", Toast.LENGTH_SHORT).show()
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = BlueNeon, contentColor = Color.Black),
                    shape = RoundedCornerShape(8.dp),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(text = "Guardar MQTT", fontWeight = FontWeight.Bold)
                }
            }
        }

        // IPs Locales
        Card(
            modifier = Modifier.fillMaxWidth().border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
            colors = CardDefaults.cardColors(containerColor = DarkSurface)
        ) {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text(text = stringResource(R.string.local_settings), fontSize = 13.sp, fontWeight = FontWeight.Bold, color = Color.White)
                OutlinedTextField(
                    value = localIpFrente,
                    onValueChange = { localIpFrente = it },
                    label = { Text("Frente Host/IP") },
                    modifier = Modifier.fillMaxWidth(),
                    colors = textFieldColors()
                )
                OutlinedTextField(
                    value = localIpPatio,
                    onValueChange = { localIpPatio = it },
                    label = { Text("Patio Host/IP") },
                    modifier = Modifier.fillMaxWidth(),
                    colors = textFieldColors()
                )
                OutlinedTextField(
                    value = localIpESP01,
                    onValueChange = { localIpESP01 = it },
                    label = { Text("ESP01 Host/IP") },
                    modifier = Modifier.fillMaxWidth(),
                    colors = textFieldColors()
                )
                Button(
                    onClick = {
                        repository.setLocalIps(localIpFrente, localIpPatio, localIpESP01)
                        Toast.makeText(context, "Servidores Locales Guardados", Toast.LENGTH_SHORT).show()
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = BlueNeon, contentColor = Color.Black),
                    shape = RoundedCornerShape(8.dp),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(text = "Guardar Servidores Locales", fontWeight = FontWeight.Bold)
                }
            }
        }


        // --- SECCIÓN CALIBRACIÓN ---
        SectionHeader(title = stringResource(R.string.sensor_settings), icon = Icons.Default.Tune, tint = OrangeAlert)

        // Deltas Climatizador
        Card(
            modifier = Modifier.fillMaxWidth().border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
            colors = CardDefaults.cardColors(containerColor = DarkSurface)
        ) {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text(text = stringResource(R.string.temp_deltas), fontSize = 13.sp, fontWeight = FontWeight.Bold, color = Color.White)
                
                Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(
                        value = diffOn,
                        onValueChange = { diffOn = it },
                        label = { Text("Encendido (ºC)") },
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        modifier = Modifier.weight(1f),
                        colors = textFieldColors()
                    )
                    OutlinedTextField(
                        value = diffOff,
                        onValueChange = { diffOff = it },
                        label = { Text("Apagado (ºC)") },
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        modifier = Modifier.weight(1f),
                        colors = textFieldColors()
                    )
                }

                Button(
                    onClick = {
                        val dOn = diffOn.toFloatOrNull() ?: 10f
                        val dOff = diffOff.toFloatOrNull() ?: 5f
                        repository.sendPatioCommands(mapOf(
                            "tempDif_on" to dOn,
                            "tempDif_off" to dOff
                        ))
                        Toast.makeText(context, "Deltas guardados", Toast.LENGTH_SHORT).show()
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = OrangeAlert, contentColor = Color.Black),
                    shape = RoundedCornerShape(8.dp),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(text = "Guardar Deltas", fontWeight = FontWeight.Bold)
                }
            }
        }

        // Tiempos de Bomba
        Card(
            modifier = Modifier.fillMaxWidth().border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
            colors = CardDefaults.cardColors(containerColor = DarkSurface)
        ) {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text(text = "Tiempos Seguridad Bomba (Minutos)", fontSize = 13.sp, fontWeight = FontWeight.Bold, color = Color.White)
                
                Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(
                        value = minOn,
                        onValueChange = { minOn = it },
                        label = { Text("Mín. ON") },
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        modifier = Modifier.weight(1f),
                        colors = textFieldColors()
                    )
                    OutlinedTextField(
                        value = minOff,
                        onValueChange = { minOff = it },
                        label = { Text("Mín. OFF") },
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        modifier = Modifier.weight(1f),
                        colors = textFieldColors()
                    )
                    OutlinedTextField(
                        value = maxOn,
                        onValueChange = { maxOn = it },
                        label = { Text("Máx. ON") },
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        modifier = Modifier.weight(1f),
                        colors = textFieldColors()
                    )
                }

                Button(
                    onClick = {
                        val mOn = (minOn.toLongOrNull() ?: 1L) * 60000L
                        val mOff = (minOff.toLongOrNull() ?: 5L) * 60000L
                        val mxOn = (maxOn.toLongOrNull() ?: 15L) * 60000L
                        repository.sendPatioCommands(mapOf(
                            "minOnTime" to mOn,
                            "minOffTime" to mOff,
                            "maxOnTime" to mxOn
                        ))
                        Toast.makeText(context, "Tiempos guardados", Toast.LENGTH_SHORT).show()
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = OrangeAlert, contentColor = Color.Black),
                    shape = RoundedCornerShape(8.dp),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(text = "Guardar Tiempos", fontWeight = FontWeight.Bold)
                }
            }
        }

        // LDR Sensor Ajustes
        Card(
            modifier = Modifier.fillMaxWidth().border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
            colors = CardDefaults.cardColors(containerColor = DarkSurface)
        ) {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text(text = stringResource(R.string.ldr_settings), fontSize = 13.sp, fontWeight = FontWeight.Bold, color = Color.White)
                
                Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(
                        value = ldrNightVal,
                        onValueChange = { ldrNightVal = it },
                        label = { Text(stringResource(R.string.ldr_night_val)) },
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        modifier = Modifier.weight(1f),
                        colors = textFieldColors()
                    )
                    OutlinedTextField(
                        value = ldrNightHist,
                        onValueChange = { ldrNightHist = it },
                        label = { Text(stringResource(R.string.ldr_night_hist)) },
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        modifier = Modifier.weight(1f),
                        colors = textFieldColors()
                    )
                }

                Button(
                    onClick = {
                        val valNoche = ldrNightVal.toIntOrNull() ?: 500
                        val hist = ldrNightHist.toIntOrNull() ?: 20
                        repository.sendPatioCommands(mapOf(
                            "ValorNoche" to valNoche,
                            "histeresisLuz" to hist
                        ))
                        Toast.makeText(context, "Umbrales LDR guardados", Toast.LENGTH_SHORT).show()
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = OrangeAlert, contentColor = Color.Black),
                    shape = RoundedCornerShape(8.dp),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(text = "Guardar Umbrales LDR", fontWeight = FontWeight.Bold)
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun textFieldColors() = OutlinedTextFieldDefaults.colors(
    focusedBorderColor = BlueNeon,
    unfocusedBorderColor = BorderColor,
    focusedLabelColor = BlueNeon,
    unfocusedLabelColor = OnSurfaceDark.copy(alpha = 0.5f),
    focusedTextColor = Color.White,
    unfocusedTextColor = Color.White,
    focusedContainerColor = DarkBackground,
    unfocusedContainerColor = DarkBackground
)
