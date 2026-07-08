package com.example.alarmaydomotica.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.alarmaydomotica.R
import com.example.alarmaydomotica.data.HomeRepository
import com.example.alarmaydomotica.theme.*
import com.example.alarmaydomotica.data.model.FrenteState
import com.example.alarmaydomotica.data.model.PatioState

@Composable
fun DomoticaScreen(
    repository: HomeRepository,
    onItemClick: (androidx.navigation3.runtime.NavKey) -> Unit,
    modifier: Modifier = Modifier
) {
    val frenteState by repository.frenteState.collectAsState()
    val patioState by repository.patioState.collectAsState()
    val esp01State by repository.esp01State.collectAsState()

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // --- SECCIÓN LUCES ---
        SectionHeader(title = stringResource(R.string.lights), icon = Icons.Default.Lightbulb, tint = AmberGold)

        // Frente Luces
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            LightControlCard(
                name = stringResource(R.string.light_frente_reflectores),
                currentConf = frenteState.reflectoresConf,
                realState = frenteState.reflectoresConf == 1,
                onConfChanged = { repository.sendFrenteCommand("Iluminacion/Reflectores", it) },
                modifier = Modifier.weight(1f)
            )
            LightControlCard(
                name = stringResource(R.string.light_vereda),
                currentConf = frenteState.luzVeredaConf,
                realState = frenteState.luzVeredaConf == 1,
                onConfChanged = { repository.sendFrenteCommand("Iluminacion/Vereda", it) },
                modifier = Modifier.weight(1f)
            )
        }

        // Patio Luces - Fila 1
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            LightControlCard(
                name = stringResource(R.string.light_patio_reflectores),
                currentConf = esp01State.canal1Conf,
                realState = esp01State.canal1ST,
                onConfChanged = { repository.sendESP01Command("canal1Conf", it) },
                modifier = Modifier.weight(1f)
            )
            LightControlCard(
                name = stringResource(R.string.light_pileta),
                currentConf = patioState.luzPiletaConf,
                realState = patioState.luzPiletaST,
                onConfChanged = { repository.sendPatioCommand("luzPiletaConf", it) },
                modifier = Modifier.weight(1f)
            )
        }

        // Patio Luces - Fila 2
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            LightControlCard(
                name = stringResource(R.string.light_galeria),
                currentConf = patioState.luzGaleriaConf,
                realState = patioState.luzGaleriaST,
                onConfChanged = { repository.sendPatioCommand("luzGaleriaConf", it) },
                modifier = Modifier.weight(1f)
            )
            LightControlCard(
                name = stringResource(R.string.light_galeria_borde),
                currentConf = esp01State.canal2Conf,
                realState = esp01State.canal2ST,
                onConfChanged = { repository.sendESP01Command("canal2Conf", it) },
                modifier = Modifier.weight(1f)
            )
        }


        Spacer(modifier = Modifier.height(8.dp))

        // --- SECCIÓN BOMBAS ---
        SectionHeader(title = stringResource(R.string.pumps), icon = Icons.Default.Water, tint = BlueNeon)

        // Estado Cisterna Nivel (Visual)
        CisternaLevelCard(patioState.nivelAgua)

        // Bomba Cisterna
        PumpToggleCard(
            name = stringResource(R.string.pump_cisterna),
            currentConf = patioState.bombaCisternaConf,
            realState = patioState.bombaCisternaST,
            onConfChanged = { repository.sendPatioCommand("bombaCisternaConf", it) }
        )

        // Bomba Tanque
        PumpToggleCard(
            name = stringResource(R.string.pump_tanque),
            currentConf = patioState.bombaTanqueConf,
            realState = patioState.bombaTanqueST,
            onConfChanged = { repository.sendPatioCommand("bombaTanqueConf", it) }
        )

        Spacer(modifier = Modifier.height(8.dp))

        // --- SECCIÓN AUXILIARES ---
        SectionHeader(title = stringResource(R.string.auxiliary), icon = Icons.Default.Extension, tint = OnSurfaceDark)

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            AuxToggleCard(
                name = stringResource(R.string.aux_1),
                enabled = patioState.aux1En,
                onCheckedChange = { repository.sendPatioCommand("IntercambioST", it) }, // Nota: IntercambioST o mapear a aux1 en tu backend
                modifier = Modifier.weight(1f)
            )
            AuxToggleCard(
                name = stringResource(R.string.aux_2),
                enabled = patioState.aux2En,
                onCheckedChange = { repository.sendPatioCommand("Telemetria", it) },
                modifier = Modifier.weight(1f)
            )
        }

        Spacer(modifier = Modifier.height(16.dp))
        SectionHeader(title = "Programación Horaria", icon = Icons.Default.AccessTime, tint = BlueNeon)
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
                    text = "Configura los horarios automáticos de encendido/apagado para tus dispositivos.",
                    color = OnSurfaceDark.copy(alpha = 0.7f),
                    fontSize = 13.sp
                )
                
                Button(
                    onClick = { onItemClick(com.example.alarmaydomotica.Programacion("Frente")) },
                    modifier = Modifier.fillMaxWidth(),
                    colors = ButtonDefaults.buttonColors(containerColor = BlueNeon, contentColor = Color.Black)
                ) {
                    Icon(Icons.Default.Schedule, contentDescription = null, modifier = Modifier.size(18.dp))
                    Spacer(modifier = Modifier.width(8.dp))
                    Text("Horarios del Frente (Luces y Alarma)", fontWeight = FontWeight.Bold, fontSize = 13.sp)
                }

                Button(
                    onClick = { onItemClick(com.example.alarmaydomotica.Programacion("Patio")) },
                    modifier = Modifier.fillMaxWidth(),
                    colors = ButtonDefaults.buttonColors(containerColor = BlueNeon, contentColor = Color.Black)
                ) {
                    Icon(Icons.Default.Schedule, contentDescription = null, modifier = Modifier.size(18.dp))
                    Spacer(modifier = Modifier.width(8.dp))
                    Text("Horarios del Patio (Luces y Bombas)", fontWeight = FontWeight.Bold, fontSize = 13.sp)
                }

                Button(
                    onClick = { onItemClick(com.example.alarmaydomotica.Programacion("ESP01")) },
                    modifier = Modifier.fillMaxWidth(),
                    colors = ButtonDefaults.buttonColors(containerColor = BlueNeon, contentColor = Color.Black)
                ) {
                    Icon(Icons.Default.Schedule, contentDescription = null, modifier = Modifier.size(18.dp))
                    Spacer(modifier = Modifier.width(8.dp))
                    Text("Horarios del ESP01 (Luces Galería)", fontWeight = FontWeight.Bold, fontSize = 13.sp)
                }
            }
        }
    }
}

@Composable
fun SectionHeader(title: String, icon: ImageVector, tint: Color) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(imageVector = icon, contentDescription = title, tint = tint, modifier = Modifier.size(22.dp))
        Spacer(modifier = Modifier.width(8.dp))
        Text(
            text = title,
            color = Color.White,
            fontSize = 18.sp,
            fontWeight = FontWeight.Bold,
            letterSpacing = 0.5.sp
        )
    }
}

@Composable
fun LightControlCard(
    name: String,
    currentConf: Int,
    realState: Boolean,
    onConfChanged: (Int) -> Unit,
    modifier: Modifier = Modifier
) {
    Card(
        modifier = modifier.border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
        colors = CardDefaults.cardColors(containerColor = DarkSurface),
        shape = RoundedCornerShape(16.dp)
    ) {
        Column(
            modifier = Modifier.padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(
                    text = name,
                    fontSize = 12.sp,
                    fontWeight = FontWeight.SemiBold,
                    color = Color.White,
                    maxLines = 1
                )
                // Indicador de luz encendida (bombilla amarilla brillante)
                Icon(
                    imageVector = Icons.Default.Lightbulb,
                    contentDescription = null,
                    tint = if (realState) AmberGold else BorderColor,
                    modifier = Modifier.size(16.dp)
                )
            }

            // Interruptor segmentado OFF / ON / AUTO (0, 1, 2)
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(6.dp))
                    .background(DarkBackground)
                    .padding(2.dp)
            ) {
                val options = listOf(
                    0 to stringResource(R.string.mode_off),
                    1 to stringResource(R.string.mode_on),
                    2 to stringResource(R.string.mode_auto)
                )
                options.forEach { (value, label) ->
                    val isSelected = currentConf == value
                    Box(
                        contentAlignment = Alignment.Center,
                        modifier = Modifier
                            .weight(1f)
                            .height(28.dp)
                            .clip(RoundedCornerShape(4.dp))
                            .background(if (isSelected) AmberGold else Color.Transparent)
                            .clickable { onConfChanged(value) }
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
fun CisternaLevelCard(nivelAgua: Boolean) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
        colors = CardDefaults.cardColors(containerColor = DarkSurface),
        shape = RoundedCornerShape(16.dp)
    ) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = stringResource(R.string.water_level),
                    fontSize = 12.sp,
                    color = OnSurfaceDark.copy(alpha = 0.7f),
                    fontWeight = FontWeight.Medium
                )
                Text(
                    text = if (nivelAgua) stringResource(R.string.water_present) else stringResource(R.string.water_empty),
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Bold,
                    color = if (nivelAgua) BlueNeon else RedPanic
                )
            }

            // Visual bar
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(14.dp)
                    .clip(RoundedCornerShape(7.dp))
                    .background(DarkBackground)
            ) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth(if (nivelAgua) 1.0f else 0.05f)
                        .fillMaxHeight()
                        .clip(RoundedCornerShape(7.dp))
                        .background(if (nivelAgua) BlueNeon else RedPanic)
                )
            }
        }
    }
}

@Composable
fun PumpToggleCard(
    name: String,
    currentConf: Int,
    realState: Boolean,
    onConfChanged: (Int) -> Unit
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
                .padding(horizontal = 16.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Column {
                Text(text = name, fontSize = 14.sp, fontWeight = FontWeight.Bold, color = Color.White)
                Text(
                    text = if (realState) "ENCENDIDA (ON)" else "APAGADA (OFF)",
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Bold,
                    color = if (realState) BlueNeon else OnSurfaceDark.copy(alpha = 0.5f)
                )
            }

            // Selector segmentado ON / OFF
            Row(
                modifier = Modifier
                    .width(110.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .background(DarkBackground)
                    .padding(2.dp)
            ) {
                val modes = listOf(0 to "OFF", 1 to "ON")
                modes.forEach { (value, label) ->
                    val isSelected = currentConf == value
                    Box(
                        contentAlignment = Alignment.Center,
                        modifier = Modifier
                            .weight(1f)
                            .height(30.dp)
                            .clip(RoundedCornerShape(4.dp))
                            .background(if (isSelected) BlueNeon else Color.Transparent)
                            .clickable { onConfChanged(value) }
                    ) {
                        Text(
                            text = label,
                            fontSize = 11.sp,
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
fun AuxToggleCard(
    name: String,
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
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Text(text = name, fontSize = 13.sp, fontWeight = FontWeight.Bold, color = Color.White)
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
