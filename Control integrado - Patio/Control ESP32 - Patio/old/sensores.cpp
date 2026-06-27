#include "sensores.h"
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

bool nivelAguaST = false;

// =====================================================
// OBJETOS DE SENSORES (internos al módulo)
// =====================================================
DHT dht(SenTyH, DHT22);
OneWire oneWireObj(pinDatoTemp);
DallasTemperature ds18(&oneWireObj);

// =====================================================
// FLAGS DE ESTADO
// =====================================================
bool dhtOK = true;
bool dsOK_pileta = true;
bool dsOK_colector = true;
bool nivelOK = true;
bool luzOK = true;

// =====================================================
// FILTROS (suavizado simple)
// =====================================================
float filtrar(float valorPrev, float lecturaNueva, float factor = 0.2)
{
    return (valorPrev * (1.0 - factor)) + (lecturaNueva * factor);
}

// =====================================================
// INICIALIZACIÓN DE SENSORES
// =====================================================
void sensoresInit()
{
    dht.begin();
    ds18.begin();
}

// =====================================================
// LECTURA SEGURA DHT22 (con reintentos)
// =====================================================
void leerDHT()
{
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t))
    {
        dhtOK = false;
        return;
    }

    dhtOK = true;
    HUMEDAD = filtrar(HUMEDAD, h);
    TEMPERATURA = filtrar(TEMPERATURA, t);
}

// =====================================================
// LECTURA SEGURA DS18B20 (2 sensores)
// =====================================================
void leerDS18()
{
    ds18.requestTemperatures();

    float t0 = ds18.getTempCByIndex(0);
    float t1 = ds18.getTempCByIndex(1);

    // Pileta ----------------------------------------
    if (t0 == DEVICE_DISCONNECTED_C || t0 < -20 || t0 > 80)
    {
        dsOK_pileta = false;
    }
    else
    {
        dsOK_pileta = true;
        tempPileta = filtrar(tempPileta, t0);
    }

    // Colector --------------------------------------
    if (t1 == DEVICE_DISCONNECTED_C || t1 < -20 || t1 > 120)
    {
        dsOK_colector = false;
    }
    else
    {
        dsOK_colector = true;
        tempColector = filtrar(tempColector, t1);
    }
}

// =====================================================
// SENSOR CAPACITIVO (ON/OFF)
// Lectura robusta con anti-flutter
// =====================================================
void leerNivel()
{
    const int muestras = 10;
    int contadorAlta = 0;

    for (int i = 0; i < muestras; i++)
    {
        if (digitalRead(nivelAgua)) contadorAlta++;
        delayMicroseconds(500);
    }

    // Si más del 70% de las muestras fueron ALTAS → hay agua
    bool nuevoEstado = (contadorAlta > (muestras * 0.7));

    // Validación del rango
    if (contadorAlta < 0 || contadorAlta > muestras)
    {
        nivelOK = false;
        return;
    }

    nivelOK = true;

    // Filtrado ON/OFF (solo cambia si se mantiene estable)
    static bool estadoPrevio = false;
    static uint32_t tiempoCambio = 0;

    if (nuevoEstado != estadoPrevio)
    {
        // Espera estado estable al menos 100 ms
        if (millis() - tiempoCambio >= 100)
        {
            estadoPrevio = nuevoEstado;
            nivelAguaST = nuevoEstado;   // ← variable global
        }
    }
    else
    {
        tiempoCambio = millis(); // Estado estable → refresca el timer
    }
}

// =====================================================
// SENSOR DE LUZ (promediado)
// =====================================================
void leerLuz()
{
    long suma = 0;
    const int muestras = 20;

    for (int i = 0; i < muestras; i++)
    {
        suma += analogRead(fotoPin);
        delayMicroseconds(200);
    }

    int lectura = suma / muestras;

    if (lectura < 0 || lectura > 4095)
    {
        luzOK = false;
        return;
    }

    luzOK = true;
    Luz = filtrar(Luz, lectura, 0.1);
}

// =====================================================
// FUNCIÓN GENERAL (llamada desde main)
// =====================================================
void leerSensores()
{
    leerDHT();
    leerDS18();
    leerNivel();
    leerLuz();
}
