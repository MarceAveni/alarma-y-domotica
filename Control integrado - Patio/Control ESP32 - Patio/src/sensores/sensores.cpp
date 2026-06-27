#include "sensores.h"
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>

// =====================================================
// OBJETOS DE SENSORES
// =====================================================
DHT dht(SenTyH, DHT22);
OneWire oneWireObj(pinDatoTemp);
DallasTemperature ds18(&oneWireObj);
Preferences prefs;

// =====================================================
// FLAGS DE ESTADO
// =====================================================
bool dhtOK = true;
bool dsOK_pileta = true;
bool dsOK_colector = true;
bool nivelOK = true;
bool luzOK = true;

// =====================================================
// GLOBALES INTERNAS
// =====================================================
DeviceAddress romPileta;
DeviceAddress romColector;
bool romsCargadas = false;
bool nivelAguaST = false;

// =====================================================
// FILTRO
// =====================================================
float filtrar(float valorPrev, float lecturaNueva, float factor = 0.2)
{
    return (valorPrev * (1.0 - factor)) + (lecturaNueva * factor);
}

// =====================================================
// FUNCIONES AUXILIARES ROM
// =====================================================

bool mismaROM(const uint8_t *a, const uint8_t *b)
{
    for (int i = 0; i < 8; i++)
        if (a[i] != b[i]) return false;
    return true;
}

bool validarROMsEnBus()
{
    int cant = ds18.getDeviceCount();
    if (cant != 2) return false;

    DeviceAddress a0, a1;
    ds18.getAddress(a0, 0);
    ds18.getAddress(a1, 1);

    bool piletaOK =
        mismaROM(a0, romPileta) || mismaROM(a1, romPileta);

    bool colectorOK =
        mismaROM(a0, romColector) || mismaROM(a1, romColector);

    return piletaOK && colectorOK;
}

void guardarROMs()
{
    prefs.begin("ds18b20", false);
    prefs.putBool("ready", true);
    prefs.putBytes("pileta", romPileta, 8);
    prefs.putBytes("colector", romColector, 8);
    prefs.end();
}

bool cargarROMs()
{
    prefs.begin("ds18b20", true);

    if (!prefs.getBool("ready", false)) {
        prefs.end();
        return false;    // nunca se guardó nada
    }

    prefs.getBytes("pileta", romPileta, 8);
    prefs.getBytes("colector", romColector, 8);
    prefs.end();
    return true;
}

void guardarIntercambioST()
{
    prefs.begin("config", false);
    prefs.putBool("swap_ds", IntercambioST);
    prefs.end();

    Serial.printf("IntercambioST guardado: %d\n", IntercambioST);
}

void cargarIntercambioST()
{
    prefs.begin("config", true);
    IntercambioST = prefs.getBool("swap_ds", false);
    prefs.end();

    Serial.printf("IntercambioST cargado: %d\n", IntercambioST);
}


// =====================================================
// Registrar sensores por ROM
// =====================================================
void registrarSensores()
{
    ds18.begin();
    int cant = ds18.getDeviceCount();

    Serial.printf("Sensores DS18B20 detectados: %d\n", cant);
    if (cant != 2) {
        Serial.println("ERROR: Deben haber exactamente DOS DS18B20");
        return;
    }

    DeviceAddress a0, a1;
    ds18.getAddress(a0, 0);
    ds18.getAddress(a1, 1);

    // Mantener consistencia siempre igual
    if (memcmp(a0, a1, 8) < 0) {
        memcpy(romPileta, a0, 8);
        memcpy(romColector, a1, 8);
    } else {
        memcpy(romPileta, a1, 8);
        memcpy(romColector, a0, 8);
    }

    guardarROMs();
    Serial.println("ROMs DS18B20 registradas.");
}

// =====================================================
// Inicialización
// =====================================================
void sensoresInit()
{
    cargarIntercambioST();   // 👈 primero esto

    dht.begin();
    ds18.begin();

    if (!cargarROMs()) {
        Serial.println("No hay ROM guardada. Registrando...");
        registrarSensores();
        romsCargadas = true;
    }
    else {
        Serial.println("ROMs DS18B20 cargadas desde memoria.");

        if (!validarROMsEnBus()) {
            Serial.println("ROMs no coinciden. Re-registrando sensores...");
            registrarSensores();
            romsCargadas = true;
        }
        else {
            romsCargadas = true;
        }
    }
}

// =====================================================
// LECTURA SEGURA DS18B20 POR ROM
// =====================================================
void leerDS18()
{
    if (!romsCargadas) return;

    ds18.requestTemperatures();

    DeviceAddress *romPile;
    DeviceAddress *romCol;

    if (IntercambioST) {
        romPile = &romColector;
        romCol  = &romPileta;
    } else {
        romPile = &romPileta;
        romCol  = &romColector;
    }

    float tPile = ds18.getTempC(*romPile);
    float tCol  = ds18.getTempC(*romCol);

    // === PILETA ===
    if (tPile == DEVICE_DISCONNECTED_C || tPile < -20 || tPile > 80) {
        dsOK_pileta = false;
    } else {
        dsOK_pileta = true;
        tempPileta = filtrar(tempPileta, tPile);
    }

    // === COLECTOR ===
    if (tCol == DEVICE_DISCONNECTED_C || tCol < -20 || tCol > 120) {
        dsOK_colector = false;
    } else {
        dsOK_colector = true;
        tempColector = filtrar(tempColector, tCol);
    }
}

// =====================================================
// LECTURA DHT22
// =====================================================
void leerDHT()
{
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
        dhtOK = false;
        return;
    }

    dhtOK = true;
    HUMEDAD = filtrar(HUMEDAD, h);
    TEMPERATURA = filtrar(TEMPERATURA, t);
}

// =====================================================
// SENSOR NIVEL CAPACITIVO
// =====================================================
void leerNivel()
{
    const int muestras = 10;
    int contadorAgua = 0;

    for (int i = 0; i < muestras; i++)
    {
        if (!digitalRead(nivelAgua))   // LOW = hay agua
            contadorAgua++;

        delayMicroseconds(500);
    }

    // Si más del 70% de las muestras indican agua
    bool nuevoEstado = (contadorAgua > (muestras * 0.7));

    nivelOK = true;

    static bool estadoPrevio = false;
    static uint32_t tCambio = 0;

    if (nuevoEstado != estadoPrevio)
    {
        if (millis() - tCambio >= 100)
        {
            estadoPrevio = nuevoEstado;
            nivelAguaST = nuevoEstado;   // true = hay agua
        }
    }
    else
    {
        tCambio = millis();
    }
}

// =====================================================
// SENSOR DE LUZ
// =====================================================
void leerLuz()
{
    long suma = 0;
    const int muestras = 20;

    for (int i = 0; i < muestras; i++) {
        suma += analogRead(fotoPin);
        delayMicroseconds(200);
    }

    int lectura = suma / muestras;

    if (lectura < 0 || lectura > 4095) {
        luzOK = false;
        return;
    }

    luzOK = true;
    Luz = filtrar(Luz, lectura, 0.1);
}

// =====================================================
// FUNCIÓN GENERAL
// =====================================================
void leerSensores()
{
    leerDHT();
    leerDS18();
    leerNivel();
    leerLuz();
}

void leerDS18_crudo()
{
    ds18.requestTemperatures();
    piletaRaw = ds18.getTempCByIndex(0);
    delay(250);
    colectorRaw = ds18.getTempCByIndex(1);
}

