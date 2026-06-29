package com.example.alarmaydomotica.data.model

import kotlinx.serialization.Serializable

@Serializable
data class FrenteState(
    val temp: Int = 0,
    val hum: Int = 0,
    val luz: String = "noche", // "día" or "noche"
    val pir1Mov: Boolean = false,
    val pir2Mov: Boolean = false,
    val countMov: Int = 0,
    val reflectoresConf: Int = 0, // 0=Off, 1=On
    val luzVeredaConf: Int = 0, // 0=Off, 1=On
    val sirenaConf: Int = 0, // 0=Off, 1=On
    val camarasEn: Boolean = true,
    val pir1En: Boolean = true,
    val pir2En: Boolean = true
)
