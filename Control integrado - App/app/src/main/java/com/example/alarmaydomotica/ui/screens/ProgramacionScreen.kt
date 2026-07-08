package com.example.alarmaydomotica.ui.screens

import android.widget.Toast
import androidx.compose.foundation.BorderStroke
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.alarmaydomotica.data.HomeRepository
import com.example.alarmaydomotica.data.model.ScheduleEvent
import com.example.alarmaydomotica.theme.*

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ProgramacionScreen(
    device: String,
    repository: HomeRepository,
    onBack: () -> Unit
) {
    val context = LocalContext.current
    
    // Observar los estados de los dispositivos para cargar los schedules actuales
    val frenteState by repository.frenteState.collectAsState()
    val patioState by repository.patioState.collectAsState()
    val esp01State by repository.esp01State.collectAsState()

    // Obtener los horarios actuales correspondientes
    val initialSchedules = remember(device, frenteState, patioState, esp01State) {
        val list = when (device) {
            "Frente" -> frenteState.schedules
            "Patio" -> patioState.schedules
            "ESP01" -> esp01State.schedules
            else -> emptyList()
        }
        // Asegurarse de tener siempre exactamente 8 elementos para poder editarlos individualmente
        val mutable = list.toMutableList()
        while (mutable.size < 8) {
            mutable.add(ScheduleEvent())
        }
        mutable.take(8)
    }

    // Estado local editable para la lista de 8 horarios
    val editableSchedules = remember { mutableStateListOf<ScheduleEvent>() }

    // Sincronizar editableSchedules cuando se carguen los initialSchedules por primera vez
    LaunchedEffect(initialSchedules) {
        if (editableSchedules.isEmpty()) {
            editableSchedules.addAll(initialSchedules)
        }
    }

    // Definir los targets (actuadores) posibles por dispositivo
    val targets = remember(device) {
        when (device) {
            "Frente" -> listOf(
                0 to "Reflectores",
                1 to "Luz Vereda",
                2 to "Sirena/Alarma"
            )
            "Patio" -> listOf(
                0 to "Reflectores",
                1 to "Luz Pileta",
                2 to "Luz Galería",
                3 to "Borde Galería",
                4 to "Bomba Pileta",
                5 to "Bomba Cisterna",
                6 to "Bomba Tanque"
            )
            "ESP01" -> listOf(
                0 to "Reflector Patio",
                1 to "Borde Galería"
            )
            else -> emptyList()
        }
    }

    val weekdaysAbbr = listOf("D", "L", "M", "M", "J", "V", "S")

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Temporizadores: $device", color = Color.White, fontWeight = FontWeight.Bold) },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.Default.ArrowBack, contentDescription = "Volver", tint = Color.White)
                    }
                },
                actions = {
                    Button(
                        onClick = {
                            repository.sendSchedules(device, editableSchedules.toList())
                            Toast.makeText(context, "Configuración de horarios enviada", Toast.LENGTH_SHORT).show()
                            onBack()
                        },
                        colors = ButtonDefaults.buttonColors(
                            containerColor = BlueNeon,
                            contentColor = Color.Black
                        ),
                        shape = RoundedCornerShape(8.dp)
                    ) {
                        Icon(Icons.Default.Save, contentDescription = "Guardar", modifier = Modifier.size(16.dp))
                        Spacer(modifier = Modifier.width(4.dp))
                        Text("Guardar", fontWeight = FontWeight.Bold)
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(containerColor = DarkSurface)
            )
        },
        containerColor = DarkBackground
    ) { innerPadding ->
        Column(
            modifier = Modifier
                .padding(innerPadding)
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Text(
                text = "Puedes programar hasta 8 reglas temporizadas. El dispositivo aplicará la acción cuando coincida el horario y el día marcado.",
                color = OnSurfaceDark.copy(alpha = 0.7f),
                fontSize = 12.sp,
                lineHeight = 16.sp
            )

            // Renderizar las 8 tarjetas
            for (index in 0 until 8) {
                if (index < editableSchedules.size) {
                    val rule = editableSchedules[index]
                    
                    ScheduleRuleCard(
                        index = index + 1,
                        rule = rule,
                        targets = targets,
                        weekdaysAbbr = weekdaysAbbr,
                        onRuleChanged = { updatedRule ->
                            editableSchedules[index] = updatedRule
                        }
                    )
                }
            }
        }
    }
}

@Composable
fun ScheduleRuleCard(
    index: Int,
    rule: ScheduleEvent,
    targets: List<Pair<Int, String>>,
    weekdaysAbbr: List<String>,
    onRuleChanged: (ScheduleEvent) -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .border(1.dp, BorderColor, RoundedCornerShape(16.dp)),
        colors = CardDefaults.cardColors(
            containerColor = if (rule.active) DarkSurface else DarkSurface.copy(alpha = 0.6f)
        ),
        shape = RoundedCornerShape(16.dp)
    ) {
        Column(
            modifier = Modifier.padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            // Fila superior: ID de la regla y Switch de habilitación
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Box(
                        modifier = Modifier
                            .size(24.dp)
                            .clip(CircleShape)
                            .background(if (rule.active) BlueNeon else BorderColor),
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            text = index.toString(),
                            color = if (rule.active) Color.Black else OnSurfaceDark,
                            fontSize = 12.sp,
                            fontWeight = FontWeight.Bold
                        )
                    }
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(
                        text = "Regla $index",
                        color = Color.White,
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Bold
                    )
                }

                Switch(
                    checked = rule.active,
                    onCheckedChange = { onRuleChanged(rule.copy(active = it)) },
                    colors = SwitchDefaults.colors(
                        checkedThumbColor = BlueNeon,
                        checkedTrackColor = BlueNeon.copy(alpha = 0.3f),
                        uncheckedThumbColor = OnSurfaceDark.copy(alpha = 0.5f),
                        uncheckedTrackColor = BorderColor
                    )
                )
            }

            if (rule.active) {
                // Selector de Horario (Hora y Minuto con Botones +/- para un diseño limpio e integrado)
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text(
                        text = "Horario:",
                        color = OnSurfaceDark.copy(alpha = 0.7f),
                        fontSize = 13.sp
                    )

                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        // Control de Horas
                        TimeStepper(
                            value = rule.hour,
                            max = 23,
                            onValueChange = { onRuleChanged(rule.copy(hour = it)) }
                        )
                        Text(":", color = Color.White, fontWeight = FontWeight.Bold, fontSize = 16.sp)
                        // Control de Minutos
                        TimeStepper(
                            value = rule.minute,
                            max = 59,
                            onValueChange = { onRuleChanged(rule.copy(minute = it)) }
                        )
                    }
                }

                // Selector de Target (Actuador)
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text(
                        text = "Dispositivo:",
                        color = OnSurfaceDark.copy(alpha = 0.7f),
                        fontSize = 13.sp
                    )

                    var expanded by remember { mutableStateOf(false) }
                    val currentTargetName = targets.firstOrNull { it.first == rule.target }?.second ?: "Seleccionar"

                    Box {
                        Button(
                            onClick = { expanded = true },
                            colors = ButtonDefaults.buttonColors(
                                containerColor = DarkBackground,
                                contentColor = Color.White
                            ),
                            border = BorderStroke(1.dp, BorderColor),
                            shape = RoundedCornerShape(8.dp),
                            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 6.dp)
                        ) {
                            Text(currentTargetName, fontSize = 12.sp)
                            Spacer(modifier = Modifier.width(4.dp))
                            Icon(Icons.Default.ArrowDropDown, contentDescription = null, modifier = Modifier.size(16.dp))
                        }

                        DropdownMenu(
                            expanded = expanded,
                            onDismissRequest = { expanded = false },
                            modifier = Modifier.background(DarkSurface)
                        ) {
                            targets.forEach { (targetId, name) ->
                                DropdownMenuItem(
                                    text = { Text(name, color = Color.White, fontSize = 12.sp) },
                                    onClick = {
                                        expanded = false
                                        onRuleChanged(rule.copy(target = targetId))
                                    }
                                )
                            }
                        }
                    }
                }

                // Selector de Acción (OFF / ON / AUTO)
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text(
                        text = "Modo:",
                        color = OnSurfaceDark.copy(alpha = 0.7f),
                        fontSize = 13.sp
                    )

                    Row(
                        modifier = Modifier
                            .clip(RoundedCornerShape(8.dp))
                            .background(DarkBackground)
                            .padding(2.dp)
                    ) {
                        val actions = listOf(
                            0 to "OFF",
                            1 to "ON",
                            2 to "AUTO"
                        )
                        actions.forEach { (actionId, label) ->
                            val isSelected = rule.action == actionId
                            Box(
                                contentAlignment = Alignment.Center,
                                modifier = Modifier
                                    .width(54.dp)
                                    .height(28.dp)
                                    .clip(RoundedCornerShape(6.dp))
                                    .background(if (isSelected) BlueNeon else Color.Transparent)
                                    .clickable { onRuleChanged(rule.copy(action = actionId)) }
                            ) {
                                Text(
                                    text = label,
                                    fontSize = 10.sp,
                                    fontWeight = FontWeight.Bold,
                                    color = if (isSelected) Color.Black else OnSurfaceDark
                                )
                            }
                        }
                    }
                }

                // Selector de Días de la Semana (Máscara de bits)
                Column(
                    modifier = Modifier.fillMaxWidth(),
                    verticalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Text(
                        text = "Días activos:",
                        color = OnSurfaceDark.copy(alpha = 0.7f),
                        fontSize = 13.sp
                    )

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        for (dayIdx in 0..6) {
                            val dayBit = 1 shl dayIdx
                            val isDaySelected = (rule.weekdays and dayBit) != 0

                            Box(
                                contentAlignment = Alignment.Center,
                                modifier = Modifier
                                    .size(34.dp)
                                    .clip(CircleShape)
                                    .background(if (isDaySelected) BlueNeon else DarkBackground)
                                    .border(
                                        1.dp,
                                        if (isDaySelected) Color.Transparent else BorderColor,
                                        CircleShape
                                    )
                                    .clickable {
                                        val newWeekdays = if (isDaySelected) {
                                            rule.weekdays and dayBit.inv()
                                        } else {
                                            rule.weekdays or dayBit
                                        }
                                        onRuleChanged(rule.copy(weekdays = newWeekdays))
                                    }
                            ) {
                                Text(
                                    text = weekdaysAbbr[dayIdx],
                                    color = if (isDaySelected) Color.Black else OnSurfaceDark,
                                    fontSize = 11.sp,
                                    fontWeight = FontWeight.Bold
                                )
                            }
                        }
                    }
                }
            } else {
                Text(
                    text = "Regla inactiva",
                    color = OnSurfaceDark.copy(alpha = 0.4f),
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Medium
                )
            }
        }
    }
}

@Composable
fun TimeStepper(
    value: Int,
    max: Int,
    onValueChange: (Int) -> Unit
) {
    Row(
        modifier = Modifier
            .border(1.dp, BorderColor, RoundedCornerShape(8.dp))
            .background(DarkBackground)
            .padding(horizontal = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        IconButton(
            onClick = {
                val newVal = if (value == 0) max else value - 1
                onValueChange(newVal)
            },
            modifier = Modifier.size(24.dp)
        ) {
            Icon(Icons.Default.Remove, contentDescription = "Menos", tint = Color.White, modifier = Modifier.size(14.dp))
        }

        Text(
            text = String.format("%02d", value),
            color = Color.White,
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.width(22.dp),
            textAlign = androidx.compose.ui.text.style.TextAlign.Center
        )

        IconButton(
            onClick = {
                val newVal = if (value == max) 0 else value + 1
                onValueChange(newVal)
            },
            modifier = Modifier.size(24.dp)
        ) {
            Icon(Icons.Default.Add, contentDescription = "Más", tint = Color.White, modifier = Modifier.size(14.dp))
        }
    }
}
