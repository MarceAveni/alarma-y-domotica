package com.example.alarmaydomotica.ui.screens

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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.alarmaydomotica.R
import com.example.alarmaydomotica.data.HomeRepository
import com.example.alarmaydomotica.theme.*
import com.example.alarmaydomotica.data.model.PatioState
import java.util.Locale

@Composable
fun ClimatizacionScreen(
    repository: HomeRepository,
    modifier: Modifier = Modifier
) {
    val patioState by repository.patioState.collectAsState()

    var sliderValue by remember { mutableFloatStateOf(patioState.tempSet) }
    
    // Sincronizar el valor del slider cuando cambia en el repositorio
    LaunchedEffect(patioState.tempSet) {
        sliderValue = patioState.tempSet
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // Dial de Temperatura Set
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .shadow(4.dp, RoundedCornerShape(24.dp))
                .border(1.dp, BorderColor, RoundedCornerShape(24.dp)),
            colors = CardDefaults.cardColors(containerColor = DarkSurface)
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(24.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                Text(
                    text = "Ajustar Consigna",
                    color = OnSurfaceDark.copy(alpha = 0.7f),
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold
                )

                // Círculo del dial
                Box(
                    contentAlignment = Alignment.Center,
                    modifier = Modifier
                        .size(140.dp)
                        .clip(CircleShape)
                        .background(DarkBackground)
                        .border(2.dp, BlueNeon, CircleShape)
                ) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Text(
                            text = String.format(Locale.US, "%.1f", sliderValue),
                            fontSize = 36.sp,
                            fontWeight = FontWeight.Bold,
                            color = Color.White
                        )
                        Text(
                            text = "ºC Set",
                            fontSize = 12.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = BlueNeon
                        )
                    }
                }

                // Slider para ajustar
                Slider(
                    value = sliderValue,
                    onValueChange = { sliderValue = it },
                    onValueChangeFinished = {
                        repository.sendPatioCommand("tempSet", sliderValue)
                    },
                    valueRange = 15f..40f,
                    steps = 49, // incrementos de 0.5 grados
                    colors = SliderDefaults.colors(
                        thumbColor = BlueNeon,
                        activeTrackColor = BlueNeon,
                        inactiveTrackColor = BorderColor
                    ),
                    modifier = Modifier.padding(horizontal = 8.dp)
                )
            }
        }

        // Sensores de Temperatura
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            TempSensorWidget(
                title = "Temperatura Agua",
                temp = patioState.tempPileta,
                icon = Icons.Default.Pool,
                modifier = Modifier.weight(1f)
            )
            TempSensorWidget(
                title = "Colector Solar",
                temp = patioState.tempColector,
                icon = Icons.Default.WbSunny,
                modifier = Modifier.weight(1f)
            )
        }

        // Diferencia Térmica
        DifferentialCard(
            tempPileta = patioState.tempPileta,
            tempColector = patioState.tempColector,
            isActive = patioState.bombaPiletaST
        )

        // Modo de Bomba Pileta
        PumpControlCard(
            currentConf = patioState.bombaPiletaConf,
            onConfChange = { conf ->
                repository.sendPatioCommand("bombaPiletaConf", conf)
            }
        )

        // Intercambio Solar Habilitado (Switch)
        IntercambioSolarCard(
            enabled = patioState.IntercambioST,
            onCheckedChange = { checked ->
                repository.sendPatioCommand("IntercambioST", checked)
            }
        )

        // Diagnóstico y Límites
        SafetyLimitsCard(patioState)
    }
}

@Composable
fun TempSensorWidget(
    title: String,
    temp: Float,
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    modifier: Modifier = Modifier
) {
    Card(
        modifier = modifier.border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
        colors = CardDefaults.cardColors(containerColor = DarkSurface),
        shape = RoundedCornerShape(16.dp)
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(
                    imageVector = icon,
                    contentDescription = title,
                    tint = BlueNeon,
                    modifier = Modifier.size(20.dp)
                )
                Spacer(modifier = Modifier.width(6.dp))
                Text(
                    text = title,
                    fontSize = 12.sp,
                    color = OnSurfaceDark.copy(alpha = 0.7f),
                    fontWeight = FontWeight.Medium
                )
            }
            Text(
                text = if (temp > 0f) String.format(Locale.US, "%.1f°C", temp) else "--",
                fontSize = 24.sp,
                fontWeight = FontWeight.Bold,
                color = Color.White
            )
        }
    }
}

@Composable
fun DifferentialCard(tempPileta: Float, tempColector: Float, isActive: Boolean) {
    val delta = tempColector - tempPileta
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
                .padding(16.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Column {
                Text(
                    text = "Diferencial de Temperatura",
                    fontSize = 12.sp,
                    color = OnSurfaceDark.copy(alpha = 0.7f)
                )
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = if (tempPileta > 0 && tempColector > 0) String.format(Locale.US, "%.1f°C", delta) else "--",
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold,
                    color = if (delta > 0) GreenEmerald else OnSurfaceDark
                )
            }

            // Indicador de Climatización Activa
            Box(
                modifier = Modifier
                    .clip(RoundedCornerShape(8.dp))
                    .background(if (isActive) BlueNeon.copy(alpha = 0.15f) else BorderColor.copy(alpha = 0.3f))
                    .padding(horizontal = 12.dp, vertical = 6.dp)
            ) {
                Text(
                    text = if (isActive) "CLIMATIZANDO" else "EN ESPERA",
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Bold,
                    color = if (isActive) BlueNeon else OnSurfaceDark.copy(alpha = 0.5f)
                )
            }
        }
    }
}

@Composable
fun PumpControlCard(
    currentConf: Int,
    onConfChange: (Int) -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
        colors = CardDefaults.cardColors(containerColor = DarkSurface),
        shape = RoundedCornerShape(16.dp)
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Text(
                text = stringResource(R.string.pump_mode),
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
                color = Color.White
            )

            // Selector segmentado
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(8.dp))
                    .background(DarkBackground),
                horizontalArrangement = Arrangement.SpaceEvenly
            ) {
                val modes = listOf(
                    0 to stringResource(R.string.mode_off),
                    1 to stringResource(R.string.mode_on),
                    2 to stringResource(R.string.mode_auto)
                )

                modes.forEach { (valIndex, valText) ->
                    val isSelected = currentConf == valIndex
                    Box(
                        contentAlignment = Alignment.Center,
                        modifier = Modifier
                            .weight(1f)
                            .height(38.dp)
                            .clip(RoundedCornerShape(8.dp))
                            .background(if (isSelected) BlueNeon else Color.Transparent)
                            .clickable { onConfChange(valIndex) }
                    ) {
                        Text(
                            text = valText,
                            fontSize = 12.sp,
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
fun IntercambioSolarCard(enabled: Boolean, onCheckedChange: (Boolean) -> Unit) {
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
                .padding(horizontal = 16.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(
                    imageVector = Icons.Default.WbSunny,
                    contentDescription = "Solar",
                    tint = AmberGold,
                    modifier = Modifier.size(22.dp)
                )
                Spacer(modifier = Modifier.width(10.dp))
                Column {
                    Text(
                        text = stringResource(R.string.intercambio_solar),
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color.White
                    )
                    Text(
                        text = "Permitir calefacción solar",
                        fontSize = 11.sp,
                        color = OnSurfaceDark.copy(alpha = 0.5f)
                    )
                }
            }

            Switch(
                checked = enabled,
                onCheckedChange = onCheckedChange,
                colors = SwitchDefaults.colors(
                    checkedThumbColor = BlueNeon,
                    checkedTrackColor = BlueNeon.copy(alpha = 0.3f),
                    uncheckedThumbColor = OnSurfaceDark.copy(alpha = 0.5f),
                    uncheckedTrackColor = BorderColor
                )
            )
        }
    }
}

@Composable
fun SafetyLimitsCard(state: PatioState) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
        colors = CardDefaults.cardColors(containerColor = DarkSurface.copy(alpha = 0.7f)),
        shape = RoundedCornerShape(16.dp)
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(
                    imageVector = Icons.Default.Timer,
                    contentDescription = "Protección",
                    tint = OrangeAlert,
                    modifier = Modifier.size(18.dp)
                )
                Spacer(modifier = Modifier.width(6.dp))
                Text(
                    text = "Límites de Seguridad Bomba",
                    fontSize = 13.sp,
                    fontWeight = FontWeight.Bold,
                    color = Color.White
                )
            }
            
            Divider(color = BorderColor, thickness = 0.5.dp)

            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text(text = "Mínimo ON:", fontSize = 11.sp, color = OnSurfaceDark.copy(alpha = 0.6f))
                Text(text = "${state.minOnTime / 60000} min", fontSize = 11.sp, color = OnSurfaceDark, fontWeight = FontWeight.SemiBold)
            }
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text(text = "Mínimo OFF:", fontSize = 11.sp, color = OnSurfaceDark.copy(alpha = 0.6f))
                Text(text = "${state.minOffTime / 60000} min", fontSize = 11.sp, color = OnSurfaceDark, fontWeight = FontWeight.SemiBold)
            }
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text(text = "Máximo ON:", fontSize = 11.sp, color = OnSurfaceDark.copy(alpha = 0.6f))
                Text(text = "${state.maxOnTime / 60000} min", fontSize = 11.sp, color = OnSurfaceDark, fontWeight = FontWeight.SemiBold)
            }
        }
    }
}
