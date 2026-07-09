package com.example.alarmaydomotica.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.alarmaydomotica.R
import com.example.alarmaydomotica.data.HomeRepository
import com.example.alarmaydomotica.theme.*
import com.example.alarmaydomotica.data.model.FrenteState
import androidx.compose.ui.layout.layout

@Composable
fun SeguridadScreen(
    repository: HomeRepository,
    onItemClick: ((androidx.navigation3.runtime.NavKey) -> Unit)? = null,
    modifier: Modifier = Modifier
) {
    val frenteState by repository.frenteState.collectAsState()
    val logs by repository.movementLogs.collectAsState()

    val isAlarmArmed = frenteState.sirenaConf == 2

    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // Tarjeta de Armado / Desarmado de Alarma
        AlarmArmCard(
            isArmed = isAlarmArmed,
            onArmedChanged = { repository.sendFrenteCommand("Alarma/Sirena", if (it) 2 else 0) }
        )

        // Control de Cámaras y Modo Sirena
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            CameraControlCard(
                enabled = frenteState.camarasEn,
                onCheckedChange = { repository.sendFrenteCommand("Alarma/Camaras", if (it) 1 else 0) },
                modifier = Modifier.weight(1.5f)
            )
            SirenModeCard(
                currentConf = frenteState.sirenaConf,
                onConfChange = { repository.sendFrenteCommand("Alarma/Sirena", it) },
                modifier = Modifier.weight(2f)
            )
        }

        // Sensores de Movimiento (Monitoreo e Inhabilitación)
        SectionHeader(title = stringResource(R.string.sensors), icon = Icons.Default.Security, tint = RedPanic)
        SensorStateCard(
            name = stringResource(R.string.sensor_pir1),
            isMovDetected = frenteState.pir1Mov,
            isEnabled = frenteState.pir1En,
            onEnabledChange = { repository.sendFrenteCommand("Alarma/PIR1En", if (it) 1 else 0) }
        )
        SensorStateCard(
            name = stringResource(R.string.sensor_pir2),
            isMovDetected = frenteState.pir2Mov,
            isEnabled = frenteState.pir2En,
            onEnabledChange = { repository.sendFrenteCommand("Alarma/PIR2En", if (it) 1 else 0) }
        )

        // Historial de Movimientos
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = stringResource(R.string.event_log),
                color = Color.White,
                fontSize = 15.sp,
                fontWeight = FontWeight.Bold
            )
            if (logs.isNotEmpty()) {
                Text(
                    text = stringResource(R.string.clear_logs),
                    color = RedPanic,
                    fontSize = 12.sp,
                    fontWeight = FontWeight.SemiBold,
                    modifier = Modifier.clickable { repository.clearLogs() }
                )
            }
        }

        // Lista de Logs de Movimientos
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .weight(1f)
                .border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
            colors = CardDefaults.cardColors(containerColor = DarkSurface)
        ) {
            if (logs.isEmpty()) {
                Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = "Sin movimientos registrados hoy",
                        color = OnSurfaceDark.copy(alpha = 0.5f),
                        fontSize = 13.sp,
                        textAlign = TextAlign.Center
                    )
                }
            } else {
                LazyColumn(
                    modifier = Modifier.padding(12.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    items(logs) { log ->
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(8.dp))
                                .background(DarkBackground)
                                .padding(10.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Icon(
                                imageVector = Icons.Default.DirectionsWalk,
                                contentDescription = "Movimiento",
                                tint = OrangeAlert,
                                modifier = Modifier.size(16.dp)
                            )
                            Spacer(modifier = Modifier.width(8.dp))
                            Text(
                                text = log,
                                color = OnSurfaceDark,
                                fontSize = 11.sp,
                                fontWeight = FontWeight.Medium
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun AlarmArmCard(isArmed: Boolean, onArmedChanged: (Boolean) -> Unit) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .shadow(4.dp, RoundedCornerShape(20.dp))
            .border(1.dp, if (isArmed) RedPanic.copy(alpha = 0.5f) else BorderColor, RoundedCornerShape(20.dp)),
        colors = CardDefaults.cardColors(containerColor = DarkSurface),
        shape = RoundedCornerShape(20.dp)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                // Escudo
                Box(
                    contentAlignment = Alignment.Center,
                    modifier = Modifier
                        .size(46.dp)
                        .clip(CircleShape)
                        .background(if (isArmed) RedPanic.copy(alpha = 0.15f) else BorderColor.copy(alpha = 0.3f))
                ) {
                    Icon(
                        imageVector = if (isArmed) Icons.Default.Shield else Icons.Default.ShieldMoon,
                        contentDescription = "Alarma",
                        tint = if (isArmed) RedPanic else OnSurfaceDark,
                        modifier = Modifier.size(24.dp)
                    )
                }
                Spacer(modifier = Modifier.width(14.dp))
                Column {
                    Text(
                        text = stringResource(R.string.alarm_status, if (isArmed) stringResource(R.string.alarm_armed) else stringResource(R.string.alarm_disarmed)),
                        fontSize = 15.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color.White
                    )
                    Text(
                        text = if (isArmed) "Monitoreo perimetral activo" else "Monitoreo desactivado",
                        fontSize = 11.sp,
                        color = OnSurfaceDark.copy(alpha = 0.5f)
                    )
                }
            }

            Switch(
                checked = isArmed,
                onCheckedChange = onArmedChanged,
                colors = SwitchDefaults.colors(
                    checkedThumbColor = RedPanic,
                    checkedTrackColor = RedPanic.copy(alpha = 0.3f),
                    uncheckedThumbColor = OnSurfaceDark.copy(alpha = 0.5f),
                    uncheckedTrackColor = BorderColor
                )
            )
        }
    }
}

@Composable
fun CameraControlCard(
    enabled: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier
) {
    Card(
        modifier = modifier.border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
        colors = CardDefaults.cardColors(containerColor = DarkSurface),
        shape = RoundedCornerShape(16.dp)
    ) {
        Column(
            modifier = Modifier.padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Text(
                text = "Alimentación Cámaras",
                fontSize = 11.sp,
                fontWeight = FontWeight.Bold,
                color = Color.White,
                maxLines = 1
            )
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(
                    text = if (enabled) "ON" else "OFF",
                    fontSize = 11.sp,
                    fontWeight = FontWeight.Bold,
                    color = if (enabled) GreenEmerald else OnSurfaceDark.copy(alpha = 0.5f)
                )
                Switch(
                    checked = enabled,
                    onCheckedChange = onCheckedChange,
                    colors = SwitchDefaults.colors(
                        checkedThumbColor = GreenEmerald,
                        checkedTrackColor = GreenEmerald.copy(alpha = 0.3f),
                        uncheckedThumbColor = OnSurfaceDark.copy(alpha = 0.5f),
                        uncheckedTrackColor = BorderColor
                    )
                )
            }
        }
    }
}

@Composable
fun SirenModeCard(
    currentConf: Int,
    onConfChange: (Int) -> Unit,
    modifier: Modifier = Modifier
) {
    Card(
        modifier = modifier.border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
        colors = CardDefaults.cardColors(containerColor = DarkSurface),
        shape = RoundedCornerShape(16.dp)
    ) {
        Column(
            modifier = Modifier.padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Text(
                text = stringResource(R.string.siren_conf),
                fontSize = 11.sp,
                fontWeight = FontWeight.Bold,
                color = Color.White,
                maxLines = 1
            )

            // Selector segmentado OFF (0) / AUTO (1)
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(6.dp))
                    .background(DarkBackground)
                    .padding(2.dp)
            ) {
                val modes = listOf(0 to "APAGADA", 2 to "AUTO")
                modes.forEach { (value, label) ->
                    val isSelected = currentConf == value
                    Box(
                        contentAlignment = Alignment.Center,
                        modifier = Modifier
                            .weight(1f)
                            .height(28.dp)
                            .clip(RoundedCornerShape(4.dp))
                            .background(if (isSelected) RedPanic else Color.Transparent)
                            .clickable { onConfChange(value) }
                    ) {
                        Text(
                            text = label,
                            fontSize = 9.sp,
                            fontWeight = FontWeight.Bold,
                            color = if (isSelected) Color.Black else OnSurfaceDark
                        )
                    }
                }
            }
        }
    }
}

@Composable
fun SensorStateCard(
    name: String,
    isMovDetected: Boolean,
    isEnabled: Boolean,
    onEnabledChange: (Boolean) -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
        colors = CardDefaults.cardColors(containerColor = DarkSurface),
        shape = RoundedCornerShape(16.dp)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(12.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(
                    modifier = Modifier
                        .size(10.dp)
                        .clip(CircleShape)
                        .background(
                            when {
                                !isEnabled -> BorderColor
                                isMovDetected -> RedPanic
                                else -> GreenEmerald
                            }
                        )
                )
                Spacer(modifier = Modifier.width(12.dp))
                Column {
                    Text(
                        text = name,
                        fontSize = 13.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color.White
                    )
                    Text(
                        text = when {
                            !isEnabled -> "ANULADO"
                            isMovDetected -> "¡MOVIMIENTO!"
                            else -> "Normal"
                        },
                        fontSize = 11.sp,
                        color = when {
                            !isEnabled -> OnSurfaceDark.copy(alpha = 0.5f)
                            isMovDetected -> RedPanic
                            else -> OnSurfaceDark.copy(alpha = 0.7f)
                        },
                        fontWeight = FontWeight.SemiBold
                    )
                }
            }

            // Anular / Habilitar Checkbox/Switch
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    text = if (isEnabled) "Habilitado" else "Anulado",
                    fontSize = 10.sp,
                    color = OnSurfaceDark.copy(alpha = 0.5f),
                    modifier = Modifier.padding(end = 8.dp)
                )
                Switch(
                    checked = isEnabled,
                    onCheckedChange = onEnabledChange,
                    colors = SwitchDefaults.colors(
                        checkedThumbColor = GreenEmerald,
                        checkedTrackColor = GreenEmerald.copy(alpha = 0.3f),
                        uncheckedThumbColor = OnSurfaceDark.copy(alpha = 0.5f),
                        uncheckedTrackColor = BorderColor
                    ),
                    modifier = Modifier.scale(0.8f)
                )
            }
        }
    }
}

// Extension to scale components easily
fun Modifier.scale(scale: Float): Modifier = this.then(
    Modifier.layout { measurable, constraints ->
        val placeable = measurable.measure(constraints)
        layout((placeable.width * scale).toInt(), (placeable.height * scale).toInt()) {
            placeable.placeRelative(0, 0)
        }
    }
)
