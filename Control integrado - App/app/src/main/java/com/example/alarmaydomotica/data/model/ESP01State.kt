package com.example.alarmaydomotica.data.model

import kotlinx.serialization.Serializable

@Serializable
data class ESP01State(
    val canal1ST: Boolean = false,
    val canal1Conf: Int = 0,
    val canal2ST: Boolean = false,
    val canal2Conf: Int = 0,
    val luzAmbiente: String = "dia",
    val intervalData: Int = 60
)
