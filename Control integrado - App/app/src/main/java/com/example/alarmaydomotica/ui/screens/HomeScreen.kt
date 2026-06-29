package com.example.alarmaydomotica.ui.screens

import android.widget.Toast
import androidx.compose.animation.core.*
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.alarmaydomotica.R
import com.example.alarmaydomotica.data.HomeRepository
import com.example.alarmaydomotica.theme.*
import com.example.alarmaydomotica.data.model.FrenteState
import com.example.alarmaydomotica.data.model.PatioState
import java.util.Locale

@Composable
fun HomeScreen(
    repository: HomeRepository,
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    val frenteState by repository.frenteState.collectAsState()
    val patioState by repository.patioState.collectAsState()
    val isMqttConnected by repository.isMqttConnected.collectAsState()
    val isLocalMode by repository.isLocalMode.collectAsState()

    var lastPanicClick by remember { mutableStateOf(0L) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // Estado de red
        NetworkStatusHeader(isMqttConnected, isLocalMode)

        // Botón de Pánico (Sirena)
        PanicButton(
            isSirenaOn = frenteState.sirenaConf == 1,
            onClick = {
                val now = System.currentTimeMillis()
                if (now - lastPanicClick < 500) {
                    repository.triggerPanic()
                    Toast.makeText(context, "Sirena conmutada", Toast.LENGTH_SHORT).show()
                } else {
                    Toast.makeText(context, "Presioná dos veces seguidas para PÁNICO", Toast.LENGTH_SHORT).show()
                }
                lastPanicClick = now
            }
        )

        // Alertas Activas
        ActiveAlertsSection(frenteState, patioState)

        // Telemetría Exterior (Frente)
        DashboardSectionTitle(title = "Exterior (Frente)")
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            TelemetryCard(
                title = stringResource(R.string.outside_temp),
                value = "${frenteState.temp}°C",
                icon = Icons.Default.Thermostat,
                color = BlueNeon,
                modifier = Modifier.weight(1f)
            )
            TelemetryCard(
                title = stringResource(R.string.outside_hum),
                value = "${frenteState.hum}%",
                icon = Icons.Default.WaterDrop,
                color = BlueNeon,
                modifier = Modifier.weight(1f)
            )
        }

        // Telemetría Patio y Pileta
        DashboardSectionTitle(title = "Patio y Pileta")
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            TelemetryCard(
                title = stringResource(R.string.pool_temp),
                value = if (patioState.tempPileta > 0) "${String.format(Locale.US, "%.1f", patioState.tempPileta)}°C" else "--",
                icon = Icons.Default.Pool,
                color = BlueNeon,
                modifier = Modifier.weight(1f)
            )
            TelemetryCard(
                title = stringResource(R.string.patio_temp),
                value = if (patioState.tempTecho > 0) "${String.format(Locale.US, "%.1f", patioState.tempTecho)}°C" else "--",
                icon = Icons.Default.Cloud,
                color = BlueNeon,
                modifier = Modifier.weight(1f)
            )
        }

        // Estado del Nivel de Agua
        WaterLevelOverviewCard(patioState.nivelAgua)
    }
}

@Composable
fun NetworkStatusHeader(isMqttConnected: Boolean, isLocalMode: Boolean) {
    val statusText = when {
        isLocalMode -> stringResource(R.string.local_connected)
        isMqttConnected -> stringResource(R.string.mqtt_connected)
        else -> stringResource(R.string.disconnected)
    }
    
    val statusColor = when {
        isLocalMode -> BlueNeon
        isMqttConnected -> GreenEmerald
        else -> RedPanic
    }

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(DarkSurface)
            .border(1.dp, BorderColor, RoundedCornerShape(12.dp))
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(
                imageVector = Icons.Default.Wifi,
                contentDescription = "Conexión",
                tint = statusColor,
                modifier = Modifier.size(20.dp)
            )
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                text = stringResource(R.string.connection_status, statusText),
                fontSize = 14.sp,
                fontWeight = FontWeight.SemiBold,
                color = OnSurfaceDark
            )
        }
        
        // Indicador de pulso
        Box(
            modifier = Modifier
                .size(10.dp)
                .clip(CircleShape)
                .background(statusColor)
        )
    }
}

@Composable
fun PanicButton(isSirenaOn: Boolean, onClick: () -> Unit) {
    val infiniteTransition = rememberInfiniteTransition(label = "pulse")
    val scale by infiniteTransition.animateFloat(
        initialValue = 1.0f,
        targetValue = if (isSirenaOn) 1.15f else 1.02f,
        animationSpec = infiniteRepeatable(
            animation = tween(800, easing = LinearEasing),
            repeatMode = RepeatMode.Reverse
        ),
        label = "scale"
    )

    val glowAlpha by infiniteTransition.animateFloat(
        initialValue = 0.2f,
        targetValue = if (isSirenaOn) 0.8f else 0.4f,
        animationSpec = infiniteRepeatable(
            animation = tween(800, easing = LinearEasing),
            repeatMode = RepeatMode.Reverse
        ),
        label = "glow"
    )

    Box(
        contentAlignment = Alignment.Center,
        modifier = Modifier
            .padding(vertical = 12.dp)
            .size(160.dp)
    ) {
        // Glow Effect
        Box(
            modifier = Modifier
                .size(140.dp * scale)
                .clip(CircleShape)
                .background(
                    Brush.radialGradient(
                        colors = listOf(
                            RedPanic.copy(alpha = glowAlpha),
                            Color.Transparent
                        )
                    )
                )
        )

        // Outer Border
        Box(
            contentAlignment = Alignment.Center,
            modifier = Modifier
                .size(120.dp)
                .clip(CircleShape)
                .background(DarkSurface)
                .border(2.dp, if (isSirenaOn) RedPanic else BorderColor, CircleShape)
                .clickable { onClick() }
        ) {
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center
            ) {
                Icon(
                    imageVector = Icons.Default.Campaign,
                    contentDescription = "Pánico",
                    tint = if (isSirenaOn) RedPanic else OnSurfaceDark,
                    modifier = Modifier.size(36.dp)
                )
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = stringResource(R.string.panic_button),
                    color = if (isSirenaOn) RedPanic else Color.White,
                    fontWeight = FontWeight.Bold,
                    fontSize = 18.sp,
                    letterSpacing = 1.sp
                )
                Text(
                    text = "Doble toque",
                    color = OnSurfaceDark.copy(alpha = 0.5f),
                    fontSize = 9.sp
                )
            }
        }
    }
}

@Composable
fun TelemetryCard(
    title: String,
    value: String,
    icon: ImageVector,
    color: Color,
    modifier: Modifier = Modifier
) {
    Card(
        modifier = modifier
            .shadow(4.dp, RoundedCornerShape(16.dp))
            .border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
        colors = CardDefaults.cardColors(containerColor = DarkSurface),
        shape = RoundedCornerShape(16.dp)
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Icon(
                imageVector = icon,
                contentDescription = title,
                tint = color,
                modifier = Modifier.size(24.dp)
            )
            Text(
                text = title,
                fontSize = 12.sp,
                color = OnSurfaceDark.copy(alpha = 0.7f),
                fontWeight = FontWeight.Medium
            )
            Text(
                text = value,
                fontSize = 22.sp,
                fontWeight = FontWeight.Bold,
                color = Color.White
            )
        }
    }
}

@Composable
fun WaterLevelOverviewCard(nivelAgua: Boolean) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .shadow(4.dp, RoundedCornerShape(16.dp))
            .border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
        colors = CardDefaults.cardColors(containerColor = DarkSurface),
        shape = RoundedCornerShape(16.dp)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Column {
                Text(
                    text = stringResource(R.string.water_level),
                    fontSize = 14.sp,
                    fontWeight = FontWeight.SemiBold,
                    color = OnSurfaceDark.copy(alpha = 0.8f)
                )
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = if (nivelAgua) stringResource(R.string.water_present) else stringResource(R.string.water_empty),
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    color = if (nivelAgua) BlueNeon else RedPanic
                )
            }

            Icon(
                imageVector = if (nivelAgua) Icons.Default.Water else Icons.Default.Warning,
                contentDescription = "Agua",
                tint = if (nivelAgua) BlueNeon else RedPanic,
                modifier = Modifier.size(32.dp)
            )
        }
    }
}

@Composable
fun ActiveAlertsSection(frenteState: FrenteState, patioState: PatioState) {
    val alerts = remember(frenteState, patioState) {
        val list = mutableListOf<String>()
        if (frenteState.pir1Mov || frenteState.pir2Mov) {
            list.add("Alarma: Movimiento detectado en el Frente")
        }
        if (!patioState.nivelAgua) {
            list.add("Crítico: Cisterna sin agua")
        }
        if (patioState.bombaPiletaST && patioState.tempPileta >= patioState.tempSet) {
            list.add("Info: Pileta alcanzó la consigna")
        }
        list
    }

    if (alerts.isNotEmpty()) {
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .border(1.dp, RedPanic.copy(alpha = 0.5f), RoundedCornerShape(16.dp)),
            colors = CardDefaults.cardColors(containerColor = RedPanic.copy(alpha = 0.15f)),
            shape = RoundedCornerShape(16.dp)
        ) {
            Column(
                modifier = Modifier.padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(
                        imageVector = Icons.Default.NotificationsActive,
                        contentDescription = "Alertas",
                        tint = RedPanic,
                        modifier = Modifier.size(20.dp)
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(
                        text = "Alertas Activas",
                        color = RedPanic,
                        fontWeight = FontWeight.Bold,
                        fontSize = 14.sp
                    )
                }
                alerts.forEach { alert ->
                    Text(
                        text = "• $alert",
                        color = OnSurfaceDark,
                        fontSize = 13.sp,
                        fontWeight = FontWeight.Medium
                    )
                }
            }
        }
    }
}

@Composable
fun DashboardSectionTitle(title: String) {
    Text(
        text = title,
        color = Color.White,
        fontSize = 16.sp,
        fontWeight = FontWeight.Bold,
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 8.dp, bottom = 4.dp),
        letterSpacing = 0.5.sp
    )
}
