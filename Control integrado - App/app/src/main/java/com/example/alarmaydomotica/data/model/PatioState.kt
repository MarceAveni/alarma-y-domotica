package com.example.alarmaydomotica.data.model

import kotlinx.serialization.Serializable

@Serializable
data class PatioState(
    val tempPileta: Float = 0f,
    val tempColector: Float = 0f,
    val bombaPiletaST: Boolean = false,
    val bombaPiletaConf: Int = 0, // 0=Off, 1=On, 2=Auto
    val tempSet: Float = 30f,
    val tempDif_on: Float = 5f,
    val tempDif_off: Float = 2f,
    val minOnTime: Long = 60000L,
    val minOffTime: Long = 300000L,
    val maxOnTime: Long = 900000L,
    val bombaCisternaST: Boolean = false,
    val bombaCisternaConf: Int = 0, // 0=Off, 1=On
    val bombaTanqueST: Boolean = false,
    val bombaTanqueConf: Int = 0, // 0=Off, 1=On
    val reflectoresST: Boolean = false,
    val reflectoresConf: Int = 0, // 0=Off, 1=On, 2=Auto
    val luzPiletaST: Boolean = false,
    val luzPiletaConf: Int = 0, // 0=Off, 1=On, 2=Auto
    val luzGaleriaST: Boolean = false,
    val luzGaleriaConf: Int = 0, // 0=Off, 1=On, 2=Auto
    val luzGaleriaBordeST: Boolean = false,
    val luzGaleriaBordeConf: Int = 0, // 0=Off, 1=On, 2=Auto
    val luzAmbiente: Int = 0,
    val nivelAgua: Boolean = false, // true = con agua
    val tempTecho: Float = 0f,
    val humTecho: Float = 0f,
    val IntercambioST: Boolean = false,
    val tempPiletaCrudo: Float = 0f,
    val tempColectorCrudo: Float = 0f,
    val ValorNoche: Int = 500,
    val histeresisLuz: Int = 20,
    val aux1En: Boolean = false,
    val aux2En: Boolean = false,
    val schedules: List<ScheduleEvent> = emptyList()
)
