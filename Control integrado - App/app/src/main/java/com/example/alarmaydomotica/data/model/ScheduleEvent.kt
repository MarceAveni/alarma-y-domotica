package com.example.alarmaydomotica.data.model

import kotlinx.serialization.Serializable

@Serializable
data class ScheduleEvent(
    val active: Boolean = false,
    val hour: Int = 0,
    val minute: Int = 0,
    val weekdays: Int = 0, // Máscara de bits: Bit 0=Dom, Bit 1=Lun, ..., Bit 6=Sab
    val target: Int = 0,
    val action: Int = 0
)
