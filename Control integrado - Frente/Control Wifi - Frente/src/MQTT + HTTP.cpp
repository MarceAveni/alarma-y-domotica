#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <ESP8266WebServer.h> // Librería para servidor HTTP
#include <EEPROM.h>
#include <time.h>
#define MQTT_MAX_PACKET_SIZE 1024


// Definición de pines
#define SenTyH 14
#define MovPIR2 13
#define MovPIR1 12
#define SalidaBz 16
#define Reflectores 4
#define Sirena 1
#define LuzVereda 5
#define Camaras 3
int fotoPin = A0;

// Prototipos de funciones
void actuadores();
void enviarDatos();
void saveConfig();

// Definición de variables y constantes --------------------------------------------------------------
int intervalData = 60;                // Intervalo de tiempo[seg] en el que se envían datos
bool Telemetria = false;              // Flag para pedir telemetría completa de todas las variables
const char *ssid = "pocha";
const char *wifiPassword = "Vale07Marce05";
const char *mqttServer = "broker.emqx.io";   //"mqtt.eclipseprojects.io";
const int mqttPort = 1883;
const char *mqttUser = "Marce";        // Reemplaza con tu usuario MQTT
const char *mqttPassword = "Aveni793"; // Reemplaza con tu contraseña MQTT
const char *EspUser = "Marce";         // Usuario para accederal ESP8266
const char *EspPassword = "EspAveni5"; // Contraseña para acceder al ESP8266

int CuentaMov = 0;
int movTime = 7;              // Tiempo [seg] de espera entre la primer detección de movimiento y la segunda
char Luz[25];
bool flagMov = false;
int var = 0;
char datos[40];
int TEMPERATURA = 0;
int HUMEDAD = 0;
int fotoval = 0;
int ValorNoche = 500;         // Valor umbral de luz para determinar si es de día o de noche
int valNocheHist = 500;       // 
int histeresisLuz = 300;
int SirenaConf = 0;           // 0 = Siempre apagada, 1 = Siempre encendida, 2 = Automático
bool sirenaST = false;        // Flag de estado de la sirena
int sirenaTime = 3;           // Tiempo [seg] de encendido de la sirena cuando se detecta movimiento
int movRst = 60;              // Tiempo [seg] de reseteo de mvimientos para activación de sirena
bool CamarasEn = true;        // true = Encendidas, false = Apagadas (para resetearlas), Están conectadas al Normal Cerrado del relé
int ReflectoresConf = 2;      // 0 = Siempre apagados, 1 = Siempre encendidos, 2 = Automático
bool reflectoresST = false;   // Flag de estado de los reflectores
int refTime = 300;            // Tiempo [seg] de encendido de los reflectores cuando se detecta movimiento
int LuzVeredaConf = 0;        // 0 = Siempre apagada, 1 = Siempre encendida, 2 = Automático
bool luzVeredaST = false;     // Flag de estado de las luces de Vereda
bool PIR1En = false;           // flag que habilita el sensor PIR1
bool PIR2En = false;           // flag que habilita el sensor PIR2
int SensorN = 0;              // Identifica que entrada de sensor se activó. 0 = Ninguno, 1 = PIR 1, 2 = PIR 2
volatile int refCount = 0;    // Contador de tiempo de encendido de reflectores
volatile int sirenaCount = 0; // Contador de tiempo de encendido de sirena
volatile int movCount = 0;    // Contador de tiempo entre primer movimiento y segundo
volatile int movRstCount = 0; //Contador de tiempo en que se resetean los movimientos contados
volatile int enviaDataCounter = 0;

Ticker secondTicker;
WiFiClient esp8266Client;
PubSubClient mqttClient(esp8266Client);
DHT dht(SenTyH, DHT11);
ESP8266WebServer server(80); // Inicializa el servidor HTTP en el puerto 80

// --- PERSISTENCIA EEPROM ---
const uint32_t EEPROM_SIGNATURE = 0x55AA7788;

struct ScheduleRule {
  bool active;
  uint8_t hour;
  uint8_t minute;
  uint8_t weekdays;  // Bit 0 = Dom, Bit 1 = Lun, ..., Bit 6 = Sab
  uint8_t target;    // 0: Reflectores, 1: Luz Vereda, 2: Sirena
  uint8_t action;    // 0: Apagado, 1: Encendido, 2: Auto
};

ScheduleRule schedules[8];
int ultimoMinutoEvaluado = -1;

// Retorna true si la hora está sincronizada
bool obtenerHoraLocal(struct tm &infoTiempo) {
  time_t ahora = time(nullptr);
  if (ahora < 1600000000) { // Septiembre 2020 aprox
    return false; // No sincronizado todavía
  }
  localtime_r(&ahora, &infoTiempo);
  return true;
}

struct Config {
  uint32_t signature;
  int SirenaConf;
  int ReflectoresConf;
  int LuzVeredaConf;
  bool CamarasEn;
  int intervalData;
  int movTime;
  int ValorNoche;
  int histeresisLuz;
  int sirenaTime;
  int refTime;
  int movRst;
  bool PIR1En;
  bool PIR2En;
  ScheduleRule schedules[8];
} config;

void loadConfig() {
  EEPROM.begin(sizeof(Config));
  EEPROM.get(0, config);
  if (config.signature != EEPROM_SIGNATURE) {
    // Configuración por defecto si la firma no coincide
    config.signature = EEPROM_SIGNATURE;
    config.SirenaConf = 0;
    config.ReflectoresConf = 2;
    config.LuzVeredaConf = 0;
    config.CamarasEn = true;
    config.intervalData = 60;
    config.movTime = 7;
    config.ValorNoche = 500;
    config.histeresisLuz = 300;
    config.sirenaTime = 3;
    config.refTime = 300;
    config.movRst = 60;
    config.PIR1En = false;
    config.PIR2En = false;
    
    // Inicializar horarios inactivos
    for (int i = 0; i < 8; i++) {
      config.schedules[i].active = false;
      config.schedules[i].hour = 0;
      config.schedules[i].minute = 0;
      config.schedules[i].weekdays = 0;
      config.schedules[i].target = 0;
      config.schedules[i].action = 0;
    }
    
    EEPROM.put(0, config);
    EEPROM.commit();
  }
  
  // Asignar variables globales
  SirenaConf = config.SirenaConf;
  ReflectoresConf = config.ReflectoresConf;
  LuzVeredaConf = config.LuzVeredaConf;
  CamarasEn = config.CamarasEn;
  intervalData = config.intervalData;
  movTime = config.movTime;
  ValorNoche = config.ValorNoche;
  histeresisLuz = config.histeresisLuz;
  sirenaTime = config.sirenaTime;
  refTime = config.refTime;
  movRst = config.movRst;
  PIR1En = config.PIR1En;
  PIR2En = config.PIR2En;
  for (int i = 0; i < 8; i++) {
    schedules[i] = config.schedules[i];
  }
}

void saveConfig() {
  config.SirenaConf = SirenaConf;
  config.ReflectoresConf = ReflectoresConf;
  config.LuzVeredaConf = LuzVeredaConf;
  config.CamarasEn = CamarasEn;
  config.intervalData = intervalData;
  config.movTime = movTime;
  config.ValorNoche = ValorNoche;
  config.histeresisLuz = histeresisLuz;
  config.sirenaTime = sirenaTime;
  config.refTime = refTime;
  config.movRst = movRst;
  config.PIR1En = PIR1En;
  config.PIR2En = PIR2En;
  for (int i = 0; i < 8; i++) {
    config.schedules[i] = schedules[i];
  }
  
  EEPROM.put(0, config);
  EEPROM.commit();
}

// --- CALLBACK MQTT ---
void callback(char *topic, byte *payload, unsigned int length)
{
  char payload_string[length + 1];
  memcpy(payload_string, payload, length);
  payload_string[length] = '\0'; // Terminar la cadena con NULL

  DynamicJsonDocument doc(1500);
  DeserializationError error = deserializeJson(doc, payload_string);
  
  if (error)
  {
    mqttClient.publish("BALH142N1788/Frente", error.c_str());
    digitalWrite(SalidaBz, HIGH);
    delay(500);
    digitalWrite(SalidaBz, LOW);
    delay(500);
    digitalWrite(SalidaBz, HIGH);
    delay(350);
    digitalWrite(SalidaBz, LOW);
    delay(350);
    digitalWrite(SalidaBz, HIGH);
    delay(200);
    digitalWrite(SalidaBz, LOW);
    return;
  }

  bool configChanged = false;

  if (doc.containsKey("ReflectoresConf"))   { ReflectoresConf = doc["ReflectoresConf"]; configChanged = true; }
  if (doc.containsKey("LuzVeredaConf"))     { LuzVeredaConf = doc["LuzVeredaConf"]; configChanged = true; }
  if (doc.containsKey("SirenaConf"))        { SirenaConf = doc["SirenaConf"]; configChanged = true; }
  if (doc.containsKey("CamarasEn"))         { CamarasEn = doc["CamarasEn"]; configChanged = true; }
  if (doc.containsKey("intervalData"))      { intervalData = doc["intervalData"]; configChanged = true; }
  if (doc.containsKey("movTime"))           { movTime = doc["movTime"]; configChanged = true; }
  if (doc.containsKey("ValorNoche"))        { ValorNoche = doc["ValorNoche"]; configChanged = true; }
  if (doc.containsKey("HisteresisLuz"))     { histeresisLuz = doc["HisteresisLuz"]; configChanged = true; }
  if (doc.containsKey("sirenaTime"))        { sirenaTime = doc["sirenaTime"]; configChanged = true; }
  if (doc.containsKey("refTime"))           { refTime = doc["refTime"]; configChanged = true; }
  if (doc.containsKey("movRst"))            { movRst = doc["movRst"]; configChanged = true; }
  if (doc.containsKey("PIR1En"))            { PIR1En = doc["PIR1En"]; configChanged = true; }
  if (doc.containsKey("PIR2En"))            { PIR2En = doc["PIR2En"]; configChanged = true; }
  if (doc.containsKey("Telemetria"))        { Telemetria = doc["Telemetria"]; }
  
  if (doc.containsKey("frenteSchedules") || doc.containsKey("schedules")) {
    JsonArray schedArr = doc.containsKey("frenteSchedules") ? doc["frenteSchedules"].as<JsonArray>() : doc["schedules"].as<JsonArray>();
    int idx = 0;
    for (JsonObject obj : schedArr) {
      if (idx >= 8) break;
      schedules[idx].active = obj["active"] | false;
      schedules[idx].hour = obj["hour"] | 0;
      schedules[idx].minute = obj["minute"] | 0;
      schedules[idx].weekdays = obj["weekdays"] | 0;
      schedules[idx].target = obj["target"] | 0;
      schedules[idx].action = obj["action"] | 0;
      idx++;
    }
    // Si enviaron menos de 8, poner el resto inactivas
    for (int i = idx; i < 8; i++) {
      schedules[i].active = false;
    }
    configChanged = true;
  }

  if (configChanged) {
    saveConfig();
    actuadores();
  }
}

void enviarDatos()
{
  DynamicJsonDocument doc(1500);

  doc["Temperatura"] = TEMPERATURA;
  doc["Humedad"] = HUMEDAD;
  doc["Luz"] = Luz;
  doc["Sensor"] = SensorN;
  doc["Movimientos"] = CuentaMov;
  doc["Camaras"] = CamarasEn;
  doc["Sirena"] = sirenaST;
  doc["Reflectores"] = reflectoresST;
  doc["Luz Vereda"] = luzVeredaST;
  doc["SirConf"] = SirenaConf;
  doc["RefConf"] = ReflectoresConf;
  doc["LuzVConf"] = LuzVeredaConf;
  doc["intervalData"] = intervalData;
  doc["movTime"] = movTime;
  doc["ValorNoche"] = ValorNoche;
  doc["HisteresisLuz"] = histeresisLuz;
  doc["ValorNocheHisteresis"] = valNocheHist;
  doc["sirenaTime"] = sirenaTime;
  doc["refTime"] = refTime;
  doc["movRst"] = movRst;
  doc["PIR1En"] = PIR1En;
  doc["PIR2En"] = PIR2En;

  JsonArray schedArr = doc.createNestedArray("schedules");
  for (int i = 0; i < 8; i++) {
    JsonObject obj = schedArr.createNestedObject();
    obj["active"] = schedules[i].active;
    obj["hour"] = schedules[i].hour;
    obj["minute"] = schedules[i].minute;
    obj["weekdays"] = schedules[i].weekdays;
    obj["target"] = schedules[i].target;
    obj["action"] = schedules[i].action;
  }

  // Convertir el documento JSON en una cadena de texto
  char buffer[1200];
  size_t n = serializeJson(doc, buffer, sizeof(buffer));

  // Publicar el JSON en el topic
  mqttClient.publish("BALH142N1788/Frente", buffer, n);
  Telemetria = false;
}

int safeRead(volatile int& var) {
  noInterrupts();
  int val = var;
  interrupts();
  return val;
}

void onSecondTick()
{
  noInterrupts();
  enviaDataCounter++;
  if (movCount  > 0)
  movCount--;
  if (refCount  > 0)
  refCount--;
  if (sirenaCount  > 0)
  sirenaCount--;
  if (movRstCount > 0)
  movRstCount--;
  interrupts();
}

void alarma()
{
  noInterrupts();
  refCount = refTime;
  movRstCount = movRst;
  if (CuentaMov <= 1) movCount = movTime;   // Cargo el contador de tiempo entre el primer movimiento y el segundo
  if (CuentaMov >= 2) sirenaCount = sirenaTime;
  interrupts();
}

void mediciones()
{
  TEMPERATURA = dht.readTemperature();
  if (isnan(TEMPERATURA))
  {
    delay(100);
    TEMPERATURA = dht.readTemperature();
    if (isnan(TEMPERATURA)) TEMPERATURA = -100;
  }

  HUMEDAD = dht.readHumidity();
  if (isnan(HUMEDAD))
  {
    delay(100);
    HUMEDAD = dht.readHumidity();
    if (isnan(HUMEDAD)) HUMEDAD = -1;
  }

  fotoval = analogRead(fotoPin);
  sprintf(Luz, "Es de %s - %d", (fotoval < valNocheHist) ? "día" : "noche", fotoval);
}

void actuadores()
{
  if (SirenaConf == 1 || (SirenaConf == 2 && safeRead(sirenaCount) > 0 ))
  {
    digitalWrite(Sirena, LOW);        // Enciende sirena
    sirenaST = true;
  }
  else 
  {
    digitalWrite(Sirena, HIGH);    // Cualquier otro caso, la apaga
    sirenaST = false;
  }

  if (ReflectoresConf == 1 || (ReflectoresConf == 2 && safeRead(refCount) > 0 && fotoval > valNocheHist))
  {
    digitalWrite(Reflectores, LOW);       // Enciende reflectores
    reflectoresST = true;
  }
  else 
  {
    digitalWrite(Reflectores, HIGH);    // Cualquier otro caso, los apaga
    reflectoresST = false;
  }

  if (LuzVeredaConf == 1 || (LuzVeredaConf == 2 && fotoval > valNocheHist))
  {
    digitalWrite(LuzVereda, LOW);       // Enciende luz de vereda
    luzVeredaST = true;
  }
  else 
  {
    digitalWrite(LuzVereda, HIGH);    // Cualquier otro caso, la apaga
    luzVeredaST = false;
  }

  digitalWrite(Camaras, CamarasEn ? HIGH : LOW);
}

// --- GESTIÓN WIFI Y MQTT NO BLOQUEANTE ---
unsigned long lastWiFiCheck = 0;
unsigned long wifiDisconnectTime = 0;

void handleWiFi() {
  unsigned long now = millis();
  if (now - lastWiFiCheck >= 1000) {
    lastWiFiCheck = now;
    if (WiFi.status() == WL_CONNECTED) {
      wifiDisconnectTime = 0; // Conectado, resetear tiempo de desconexión
    } else {
      if (wifiDisconnectTime == 0) {
        wifiDisconnectTime = now; // Guardar momento inicial de la desconexión
      } else if (now - wifiDisconnectTime > 300000) { // 5 minutos
        // Reiniciar si pasa demasiado tiempo sin conexión
        ESP.restart();
      }
    }
  }
}

unsigned long lastMqttRetry = 0;
String mqttClientId = "";

void generateMqttClientId() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char clientBuf[30];
  sprintf(clientBuf, "esp8266_Frente_%02x%02x%02x", mac[3], mac[4], mac[5]);
  mqttClientId = String(clientBuf);
}

void handleMqtt() {
  if (WiFi.status() != WL_CONNECTED) return; // Si no hay wifi, no intentar conectar
  
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttRetry >= 10000) { // Reintentar cada 10 segundos
      lastMqttRetry = now;
      if (mqttClientId == "") {
        generateMqttClientId();
      }
      if (mqttClient.connect(mqttClientId.c_str(), mqttUser, mqttPassword)) {
        mqttClient.subscribe("BALH142N1788/Aveni793");
      }
    }
  } else {
    mqttClient.loop();
  }
}

// --- CONTENIDO HTML DEL DASHBOARD (PROGMEM) ---
//#define OTA_MINIMO 1

#ifdef OTA_MINIMO
const char INDEX_HTML[] PROGMEM = "M";
#else
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta name="description" content="Panel de Control Local Alarma Frente">
    <title>Control Frente - Local Dashboard</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-color: #0f172a;
            --card-bg: rgba(30, 41, 59, 0.7);
            --border-color: rgba(255, 255, 255, 0.08);
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
            --primary: #6366f1;
            --primary-hover: #4f46e5;
            --accent-cyan: #06b6d4;
            --accent-green: #10b981;
            --accent-red: #ef4444;
            --accent-orange: #f97316;
        }
        
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        
        body {
            font-family: 'Inter', sans-serif;
            background-color: var(--bg-color);
            background-image: 
                radial-gradient(at 0% 0%, rgba(99, 102, 241, 0.1) 0px, transparent 50%),
                radial-gradient(at 100% 100%, rgba(6, 182, 212, 0.1) 0px, transparent 50%);
            color: var(--text-main);
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 20px;
        }

        header {
            width: 100%;
            max-width: 1000px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 24px;
            padding: 12px 20px;
            background: var(--card-bg);
            backdrop-filter: blur(10px);
            border: 1px solid var(--border-color);
            border-radius: 16px;
        }

        h1 {
            font-size: 1.35rem;
            font-weight: 600;
            background: linear-gradient(135deg, #fff 0%, var(--text-muted) 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }

        .status-badge {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 0.875rem;
            font-weight: 500;
            color: var(--text-muted);
            background: rgba(0, 0, 0, 0.25);
            padding: 6px 14px;
            border-radius: 9999px;
            border: 1px solid rgba(255,255,255,0.05);
        }

        .status-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background-color: var(--accent-red);
            box-shadow: 0 0 8px var(--accent-red);
            transition: all 0.3s ease;
        }

        .status-dot.online {
            background-color: var(--accent-green);
            box-shadow: 0 0 8px var(--accent-green);
        }

        .grid {
            width: 100%;
            max-width: 1000px;
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin-bottom: 24px;
        }

        .card {
            background: var(--card-bg);
            backdrop-filter: blur(10px);
            border: 1px solid var(--border-color);
            border-radius: 20px;
            padding: 24px;
            box-shadow: 0 10px 25px -5px rgba(0,0,0,0.3);
            display: flex;
            flex-direction: column;
            gap: 20px;
        }

        .card-title {
            font-size: 1.1rem;
            font-weight: 600;
            color: #fff;
            border-bottom: 1px solid rgba(255,255,255,0.05);
            padding-bottom: 12px;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }

        .telemetry-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 6px 0;
        }

        .telemetry-label {
            color: var(--text-muted);
            font-size: 0.95rem;
        }

        .telemetry-value {
            font-weight: 600;
            font-size: 1.05rem;
            color: #fff;
        }

        /* Toggles/Checks */
        .toggle-container {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 2px 0;
        }

        .switch {
            position: relative;
            display: inline-block;
            width: 48px;
            height: 24px;
        }

        .switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }

        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #334155;
            transition: .3s;
            border-radius: 24px;
        }

        .slider:before {
            position: absolute;
            content: "";
            height: 18px;
            width: 18px;
            left: 3px;
            bottom: 3px;
            background-color: white;
            transition: .3s;
            border-radius: 50%;
        }

        input:checked + .slider {
            background-color: var(--primary);
        }

        input:checked + .slider:before {
            transform: translateX(24px);
        }

        /* Segmented buttons */
        .segmented-control {
            display: flex;
            background: #1e293b;
            border-radius: 12px;
            padding: 3px;
            border: 1px solid rgba(255,255,255,0.05);
        }

        .segmented-control input {
            display: none;
        }

        .segmented-control label {
            flex: 1;
            text-align: center;
            padding: 8px 0;
            font-size: 0.8rem;
            font-weight: 500;
            color: var(--text-muted);
            cursor: pointer;
            border-radius: 9px;
            transition: all 0.2s ease;
            user-select: none;
        }

        .segmented-control input:checked + label {
            background: var(--primary);
            color: #fff;
            box-shadow: 0 4px 6px -1px rgba(0,0,0,0.2);
        }

        /* Inputs */
        .input-group {
            display: grid;
            grid-template-columns: 2fr 1fr;
            align-items: center;
            gap: 12px;
            padding: 2px 0;
        }

        .input-group label {
            font-size: 0.9rem;
            color: var(--text-muted);
        }

        .input-group input {
            background: #1e293b;
            border: 1px solid var(--border-color);
            border-radius: 8px;
            color: #fff;
            padding: 8px 12px;
            font-size: 0.9rem;
            text-align: right;
            width: 100%;
            transition: border-color 0.2s;
        }

        .input-group input:focus {
            outline: none;
            border-color: var(--primary);
        }

        /* Status Dot Indicator for Output states */
        .indicator {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 0.9rem;
            font-weight: 500;
            color: var(--text-muted);
        }

        .indicator-dot {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background-color: #475569;
            transition: all 0.2s;
        }

        .indicator-dot.active {
            background-color: var(--accent-cyan);
            box-shadow: 0 0 10px var(--accent-cyan);
        }
        
        .indicator-dot.active-red {
            background-color: var(--accent-red);
            box-shadow: 0 0 10px var(--accent-red);
        }
        
        .indicator-dot.active-orange {
            background-color: var(--accent-orange);
            box-shadow: 0 0 10px var(--accent-orange);
        }

        /* Toast notifications */
        #toast {
            visibility: hidden;
            min-width: 250px;
            background-color: rgba(30, 41, 59, 0.95);
            border: 1px solid var(--accent-green);
            color: #fff;
            text-align: center;
            border-radius: 12px;
            padding: 16px;
            position: fixed;
            z-index: 1000;
            bottom: 30px;
            box-shadow: 0 10px 15px -3px rgba(0,0,0,0.5);
            backdrop-filter: blur(10px);
            transition: opacity 0.3s ease, visibility 0.3s;
            opacity: 0;
            font-weight: 500;
        }

        #toast.show {
            visibility: visible;
            opacity: 1;
        }
        
        footer {
            margin-top: auto;
            color: var(--text-muted);
            font-size: 0.8rem;
            padding: 20px 0;
            font-size: 0.8rem;
        }
        .day-btn {
            width: 32px;
            height: 32px;
            border-radius: 50%;
            border: 1px solid var(--border-color);
            background: rgba(0,0,0,0.2);
            color: var(--text-muted);
            font-size: 0.8rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s;
            display: inline-flex;
            align-items: center;
            justify-content: center;
        }
        .day-btn.active {
            background: var(--primary);
            color: #fff;
            border-color: transparent;
            box-shadow: 0 0 6px var(--primary);
        }
    </style>
</head>
<body>
    <header>
        <h1>Control Frente</h1>
        <div class="status-badge">
            <span id="conn-dot" class="status-dot"></span>
            <span id="conn-text">Desconectado</span>
        </div>
    </header>

    <div class="grid">
        <!-- Telemetry Card -->
        <div class="card">
            <div class="card-title">
                <span>Telemetría</span>
            </div>
            <div class="telemetry-row">
                <span class="telemetry-label">Temperatura</span>
                <span id="val-temp" class="telemetry-value">--°C</span>
            </div>
            <div class="telemetry-row">
                <span class="telemetry-label">Humedad</span>
                <span id="val-hum" class="telemetry-value">--%</span>
            </div>
            <div class="telemetry-row">
                <span class="telemetry-label">Luz Ambiente</span>
                <span id="val-luz" class="telemetry-value">--</span>
            </div>
            <div class="telemetry-row">
                <span class="telemetry-label">Movimientos Totales</span>
                <span id="val-mov" class="telemetry-value">--</span>
            </div>
        </div>

        <!-- Actuators Card -->
        <div class="card">
            <div class="card-title">
                <span>Estado de Actuadores</span>
            </div>
            <div class="telemetry-row">
                <span class="telemetry-label">Sirena</span>
                <div class="indicator">
                    <span id="st-sirena" class="indicator-dot"></span>
                    <span id="txt-sirena">Apagada</span>
                </div>
            </div>
            <div class="telemetry-row">
                <span class="telemetry-label">Reflectores</span>
                <div class="indicator">
                    <span id="st-reflectores" class="indicator-dot"></span>
                    <span id="txt-reflectores">Apagados</span>
                </div>
            </div>
            <div class="telemetry-row">
                <span class="telemetry-label">Luz Vereda</span>
                <div class="indicator">
                    <span id="st-vereda" class="indicator-dot"></span>
                    <span id="txt-vereda">Apagada</span>
                </div>
            </div>
        </div>

        <!-- Controls Card -->
        <div class="card">
            <div class="card-title">
                <span>Controles Manuales</span>
            </div>
            
            <div class="toggle-container">
                <span class="telemetry-label">Habilitar PIR 1</span>
                <label class="switch">
                    <input type="checkbox" id="ctrl-pir1" onchange="sendConfig('pir1_en', this.checked)">
                    <span class="slider"></span>
                </label>
            </div>
            
            <div class="toggle-container">
                <span class="telemetry-label">Habilitar PIR 2</span>
                <label class="switch">
                    <input type="checkbox" id="ctrl-pir2" onchange="sendConfig('pir2_en', this.checked)">
                    <span class="slider"></span>
                </label>
            </div>

            <div class="toggle-container">
                <span class="telemetry-label">Alimentación Cámaras</span>
                <label class="switch">
                    <input type="checkbox" id="ctrl-camaras" onchange="sendConfig('camaras_en', this.checked)">
                    <span class="slider"></span>
                </label>
            </div>

            <div style="display: flex; flex-direction: column; gap: 8px;">
                <span class="telemetry-label">Modo Reflectores</span>
                <div class="segmented-control">
                    <input type="radio" id="ref-0" name="ref_mode" value="0" onchange="sendConfig('reflectores_conf', 0)">
                    <label for="ref-0">OFF</label>
                    <input type="radio" id="ref-1" name="ref_mode" value="1" onchange="sendConfig('reflectores_conf', 1)">
                    <label for="ref-1">ON</label>
                    <input type="radio" id="ref-2" name="ref_mode" value="2" onchange="sendConfig('reflectores_conf', 2)">
                    <label for="ref-2">AUTO</label>
                </div>
            </div>

            <div style="display: flex; flex-direction: column; gap: 8px;">
                <span class="telemetry-label">Modo Sirena</span>
                <div class="segmented-control">
                    <input type="radio" id="sir-0" name="sir_mode" value="0" onchange="sendConfig('sirena_conf', 0)">
                    <label for="sir-0">OFF</label>
                    <input type="radio" id="sir-1" name="sir_mode" value="1" onchange="sendConfig('sirena_conf', 1)">
                    <label for="sir-1">ON</label>
                    <input type="radio" id="sir-2" name="sir_mode" value="2" onchange="sendConfig('sirena_conf', 2)">
                    <label for="sir-2">AUTO</label>
                </div>
            </div>

            <div style="display: flex; flex-direction: column; gap: 8px;">
                <span class="telemetry-label">Modo Luz Vereda</span>
                <div class="segmented-control">
                    <input type="radio" id="ver-0" name="ver_mode" value="0" onchange="sendConfig('luz_vereda_conf', 0)">
                    <label for="ver-0">OFF</label>
                    <input type="radio" id="ver-1" name="ver_mode" value="1" onchange="sendConfig('luz_vereda_conf', 1)">
                    <label for="ver-1">ON</label>
                    <input type="radio" id="ver-2" name="ver_mode" value="2" onchange="sendConfig('luz_vereda_conf', 2)">
                    <label for="ver-2">AUTO</label>
                </div>
            </div>
        </div>

        <!-- Settings Card -->
        <div class="card">
            <div class="card-title">
                <span>Parámetros y Tiempos</span>
            </div>
            
            <div class="input-group">
                <label for="set-luz">Umbral Noche (LDR)</label>
                <input type="number" id="set-luz" onblur="sendConfig('valor_noche', parseInt(this.value))">
            </div>

            <div class="input-group">
                <label for="set-hist">Histéresis Luz</label>
                <input type="number" id="set-hist" onblur="sendConfig('histeresis_luz', parseInt(this.value))">
            </div>

            <div class="input-group">
                <label for="set-sertime">Tiempo Sirena (seg)</label>
                <input type="number" id="set-sertime" onblur="sendConfig('sirena_time', parseInt(this.value))">
            </div>

            <div class="input-group">
                <label for="set-reftime">Tiempo Reflectores (seg)</label>
                <input type="number" id="set-reftime" onblur="sendConfig('ref_time', parseInt(this.value))">
            </div>

            <div class="input-group">
                <label for="set-movtime">Ventana PIR (seg)</label>
                <input type="number" id="set-movtime" onblur="sendConfig('mov_time', parseInt(this.value))">
            </div>

            <div class="input-group">
                <label for="set-movrst">Reset Movimiento (seg)</label>
                <input type="number" id="set-movrst" onblur="sendConfig('mov_rst', parseInt(this.value))">
            </div>

            <div class="input-group">
                <label for="set-int">Intervalo Telemetría (seg)</label>
                <input type="number" id="set-int" onblur="sendConfig('interval_data', parseInt(this.value))">
            </div>
        </div>

        <!-- Tarjeta Temporizadores Horarios -->
        <div class="card" style="grid-column: 1 / -1;">
            <div class="card-title">
                <span>Programación Horaria (Temporizadores)</span>
            </div>
            <div style="display: flex; gap: 12px; align-items: center; margin-bottom: 8px;">
                <span class="telemetry-label">Seleccionar Regla:</span>
                <select id="select-regla" onchange="cargarReglaEnUI(this.value)" style="background: rgba(0,0,0,0.4); color: #fff; border: 1px solid var(--border-color); padding: 6px 12px; border-radius: 8px; font-family: inherit; font-size: 0.9rem;">
                    <option value="0">Regla 1</option>
                    <option value="1">Regla 2</option>
                    <option value="2">Regla 3</option>
                    <option value="3">Regla 4</option>
                    <option value="4">Regla 5</option>
                    <option value="5">Regla 6</option>
                    <option value="6">Regla 7</option>
                    <option value="7">Regla 8</option>
                </select>
                <span id="regla-active-indicator" style="font-size: 0.8rem; font-weight: 600; padding: 2px 8px; border-radius: 6px;">--</span>
            </div>

            <div id="editor-regla" style="border: 1px solid var(--border-color); padding: 16px; border-radius: 12px; background: rgba(0,0,0,0.15); display: flex; flex-direction: column; gap: 12px;">
                <div class="toggle-container">
                    <span class="telemetry-label">Regla Activa</span>
                    <label class="switch">
                        <input type="checkbox" id="regla-active" onchange="actualizarReglaLocal()">
                        <span class="slider"></span>
                    </label>
                </div>
                
                <div class="telemetry-row">
                    <span class="telemetry-label">Hora de Ejecución:</span>
                    <div style="display: flex; gap: 4px; align-items: center;">
                        <input type="number" id="regla-hora" min="0" max="23" placeholder="HH" onchange="actualizarReglaLocal()" style="width: 60px; text-align: center; background: rgba(0,0,0,0.4); color: #fff; border: 1px solid var(--border-color); padding: 4px; border-radius: 6px;">
                        <span style="color: var(--text-muted)">:</span>
                        <input type="number" id="regla-minuto" min="0" max="59" placeholder="MM" onchange="actualizarReglaLocal()" style="width: 60px; text-align: center; background: rgba(0,0,0,0.4); color: #fff; border: 1px solid var(--border-color); padding: 4px; border-radius: 6px;">
                    </div>
                </div>

                <div class="telemetry-row">
                    <span class="telemetry-label">Dispositivo Destino:</span>
                    <select id="regla-target" onchange="actualizarReglaLocal()" style="background: rgba(0,0,0,0.4); color: #fff; border: 1px solid var(--border-color); padding: 6px; border-radius: 6px; font-family: inherit;">
                        <option value="0">Reflectores</option>
                        <option value="1">Luz Vereda</option>
                        <option value="2">Sirena/Alarma</option>
                    </select>
                </div>

                <div class="telemetry-row">
                    <span class="telemetry-label">Modo / Acción:</span>
                    <select id="regla-action" onchange="actualizarReglaLocal()" style="background: rgba(0,0,0,0.4); color: #fff; border: 1px solid var(--border-color); padding: 6px; border-radius: 6px; font-family: inherit;">
                        <option value="0">Apagado (OFF)</option>
                        <option value="1">Encendido (ON)</option>
                        <option value="2">Automático (AUTO)</option>
                    </select>
                </div>

                <div style="display: flex; flex-direction: column; gap: 6px;">
                    <span class="control-label">Días Activos</span>
                    <div style="display: flex; justify-content: space-between; gap: 4px;">
                        <button type="button" class="day-btn" onclick="toggleDiaRegla(0)">D</button>
                        <button type="button" class="day-btn" onclick="toggleDiaRegla(1)">L</button>
                        <button type="button" class="day-btn" onclick="toggleDiaRegla(2)">M</button>
                        <button type="button" class="day-btn" onclick="toggleDiaRegla(3)">M</button>
                        <button type="button" class="day-btn" onclick="toggleDiaRegla(4)">J</button>
                        <button type="button" class="day-btn" onclick="toggleDiaRegla(5)">V</button>
                        <button type="button" class="day-btn" onclick="toggleDiaRegla(6)">S</button>
                    </div>
                </div>
            </div>

            <button onclick="guardarTodosLosHorarios()" style="width: 100%; background: var(--primary); color: #fff; border: none; padding: 10px; border-radius: 10px; font-weight: 600; cursor: pointer; transition: background 0.2s; font-family: inherit;">
                Guardar Todos los Horarios
            </button>
        </div>
    </div>

    <div id="toast">Configuración Guardada ✓</div>

    <footer>
        Alarma Residencial - Los Hornos
    </footer>

    <script>
        function showToast() {
            const toast = document.getElementById("toast");
            toast.className = "show";
            setTimeout(() => { toast.className = toast.className.replace("show", ""); }, 2000);
        }

        async function pollStatus() {
            try {
                const response = await fetch('/api/status');
                if (!response.ok) throw new Error();
                const data = await response.json();
                
                // Actualizar badges de conexión
                document.getElementById("conn-dot").className = "status-dot online";
                document.getElementById("conn-text").innerText = "Conectado";

                // Telemetría
                document.getElementById("val-temp").innerText = data.temp + "°C";
                document.getElementById("val-hum").innerText = data.hum + "%";
                document.getElementById("val-luz").innerText = data.luz + " (" + data.luz_txt + ")";
                document.getElementById("val-mov").innerText = data.mov_count;

                // Toggles
                document.getElementById("ctrl-pir1").checked = data.pir1_en;
                document.getElementById("ctrl-pir2").checked = data.pir2_en;
                document.getElementById("ctrl-camaras").checked = data.camaras_en;

                // Modos (Radios)
                document.getElementById("ref-" + data.reflectores_conf).checked = true;
                document.getElementById("sir-" + data.sirena_conf).checked = true;
                document.getElementById("ver-" + data.luz_vereda_conf).checked = true;

                // Estados Físicos Actuadores
                const stSirena = document.getElementById("st-sirena");
                const txtSirena = document.getElementById("txt-sirena");
                if (data.sirena_st) {
                    stSirena.className = "indicator-dot active-red";
                    txtSirena.innerText = "SONANDO";
                    txtSirena.style.color = "var(--accent-red)";
                } else {
                    stSirena.className = "indicator-dot";
                    txtSirena.innerText = "Apagada";
                    txtSirena.style.color = "var(--text-muted)";
                }

                const stRef = document.getElementById("st-reflectores");
                const txtRef = document.getElementById("txt-reflectores");
                if (data.reflectores_st) {
                    stRef.className = "indicator-dot active";
                    txtRef.innerText = "ENCENDIDOS";
                    txtRef.style.color = "var(--accent-cyan)";
                } else {
                    stRef.className = "indicator-dot";
                    txtRef.innerText = "Apagados";
                    txtRef.style.color = "var(--text-muted)";
                }

                const stVereda = document.getElementById("st-vereda");
                const txtVereda = document.getElementById("txt-vereda");
                if (data.luz_vereda_st) {
                    stVereda.className = "indicator-dot active-orange";
                    txtVereda.innerText = "ENCENDIDA";
                    txtVereda.style.color = "var(--accent-orange)";
                } else {
                    stVereda.className = "indicator-dot";
                    txtVereda.innerText = "Apagada";
                    txtVereda.style.color = "var(--text-muted)";
                }

                // Inputs de Parámetros (sólo si no tienen foco activo)
                updateInputIfNotActive("set-luz", data.valor_noche);
                updateInputIfNotActive("set-hist", data.histeresis_luz);
                updateInputIfNotActive("set-sertime", data.sirena_time);
                updateInputIfNotActive("set-reftime", data.ref_time);
                updateInputIfNotActive("set-movtime", data.mov_time);
                updateInputIfNotActive("set-movrst", data.mov_rst);
                updateInputIfNotActive("set-int", data.interval_data);

                if (!schedulesInitialized && data.schedules) {
                    inicializarHorarios(data.schedules);
                    schedulesInitialized = true;
                }

            } catch (e) {
                document.getElementById("conn-dot").className = "status-dot";
                document.getElementById("conn-text").innerText = "Desconectado";
            }
        }

        let localSchedules = [];
        let currentReglaIdx = 0;
        let schedulesInitialized = false;

        function inicializarHorarios(schedulesFromApi) {
            localSchedules = schedulesFromApi || [];
            while (localSchedules.length < 8) {
                localSchedules.push({ active: false, hour: 0, minute: 0, weekdays: 0, target: 0, action: 0 });
            }
            cargarReglaEnUI(currentReglaIdx);
        }

        function cargarReglaEnUI(idx) {
            currentReglaIdx = parseInt(idx);
            const rule = localSchedules[currentReglaIdx];
            if (!rule) return;
            
            document.getElementById("regla-active").checked = rule.active;
            document.getElementById("regla-hora").value = rule.hour;
            document.getElementById("regla-minuto").value = rule.minute;
            document.getElementById("regla-target").value = rule.target;
            document.getElementById("regla-action").value = rule.action;

            const ind = document.getElementById("regla-active-indicator");
            if (rule.active) {
                ind.innerText = "ACTIVA";
                ind.style.background = "rgba(16, 185, 129, 0.2)";
                ind.style.color = "var(--accent-green)";
            } else {
                ind.innerText = "INACTIVA";
                ind.style.background = "rgba(255, 255, 255, 0.05)";
                ind.style.color = "var(--text-muted)";
            }

            // Actualizar botones de días
            const btns = document.querySelectorAll(".day-btn");
            btns.forEach((btn, dIdx) => {
                const bit = 1 << dIdx;
                if ((rule.weekdays & bit) !== 0) {
                    btn.classList.add("active");
                } else {
                    btn.classList.remove("active");
                }
            });
        }

        function actualizarReglaLocal() {
            const rule = localSchedules[currentReglaIdx];
            if (!rule) return;
            rule.active = document.getElementById("regla-active").checked;
            rule.hour = parseInt(document.getElementById("regla-hora").value) || 0;
            rule.minute = parseInt(document.getElementById("regla-minuto").value) || 0;
            rule.target = parseInt(document.getElementById("regla-target").value) || 0;
            rule.action = parseInt(document.getElementById("regla-action").value) || 0;
            
            const ind = document.getElementById("regla-active-indicator");
            if (rule.active) {
                ind.innerText = "ACTIVA";
                ind.style.background = "rgba(16, 185, 129, 0.2)";
                ind.style.color = "var(--accent-green)";
            } else {
                ind.innerText = "INACTIVA";
                ind.style.background = "rgba(255, 255, 255, 0.05)";
                ind.style.color = "var(--text-muted)";
            }
        }

        function toggleDiaRegla(dIdx) {
            const rule = localSchedules[currentReglaIdx];
            if (!rule) return;
            const bit = 1 << dIdx;
            rule.weekdays ^= bit;
            cargarReglaEnUI(currentReglaIdx);
        }

        async function guardarTodosLosHorarios() {
            try {
                const response = await fetch('/api/config', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ schedules: localSchedules })
                });
                if (response.ok) {
                    showToast();
                    pollStatus();
                }
            } catch (e) {
                console.error("Fallo al guardar horarios");
            }
        }

        function updateInputIfNotActive(id, val) {
            const el = document.getElementById(id);
            if (document.activeElement !== el) {
                el.value = val;
            }
        }

        async function sendConfig(key, value) {
            try {
                const payload = {};
                payload[key] = value;
                const response = await fetch('/api/config', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(payload)
                });
                if (response.ok) {
                    showToast();
                    pollStatus();
                }
            } catch (e) {
                console.error("Fallo al guardar config");
            }
        }

        pollStatus();
        setInterval(pollStatus, 3000);
    </script>
</body>
</html>
)rawliteral";
#endif

// --- API WEB SERVER HANDLERS ---
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

void handleApiStatus() {
  DynamicJsonDocument doc(1500);
  doc["temp"] = TEMPERATURA;
  doc["hum"] = HUMEDAD;
  doc["luz"] = fotoval;
  doc["luz_txt"] = (fotoval < valNocheHist) ? "Dia" : "Noche";
  doc["sensor_trigger"] = SensorN;
  doc["mov_count"] = CuentaMov;
  
  doc["sirena_st"] = sirenaST;
  doc["reflectores_st"] = reflectoresST;
  doc["luz_vereda_st"] = luzVeredaST;
  
  doc["sirena_conf"] = SirenaConf;
  doc["reflectores_conf"] = ReflectoresConf;
  doc["luz_vereda_conf"] = LuzVeredaConf;
  doc["camaras_en"] = CamarasEn;
  doc["pir1_en"] = PIR1En;
  doc["pir2_en"] = PIR2En;
  
  doc["interval_data"] = intervalData;
  doc["mov_time"] = movTime;
  doc["sirena_time"] = sirenaTime;
  doc["ref_time"] = refTime;
  doc["mov_rst"] = movRst;
  doc["valor_noche"] = ValorNoche;
  doc["histeresis_luz"] = histeresisLuz;

  JsonArray schedArr = doc.createNestedArray("schedules");
  for (int i = 0; i < 8; i++) {
    JsonObject obj = schedArr.createNestedObject();
    obj["active"] = schedules[i].active;
    obj["hour"] = schedules[i].hour;
    obj["minute"] = schedules[i].minute;
    obj["weekdays"] = schedules[i].weekdays;
    obj["target"] = schedules[i].target;
    obj["action"] = schedules[i].action;
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleApiConfig() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Body vacio\"}");
    return;
  }
  
  String body = server.arg("plain");
  DynamicJsonDocument doc(1500);
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    server.send(400, "application/json", "{\"error\":\"JSON Invalido\"}");
    return;
  }
  
  bool configChanged = false;
  
  if (doc.containsKey("reflectores_conf")) { ReflectoresConf = doc["reflectores_conf"]; configChanged = true; }
  if (doc.containsKey("luz_vereda_conf"))   { LuzVeredaConf = doc["luz_vereda_conf"]; configChanged = true; }
  if (doc.containsKey("sirena_conf"))        { SirenaConf = doc["sirena_conf"]; configChanged = true; }
  if (doc.containsKey("camaras_en"))         { CamarasEn = doc["camaras_en"]; configChanged = true; }
  if (doc.containsKey("interval_data"))      { intervalData = doc["interval_data"]; configChanged = true; }
  if (doc.containsKey("mov_time"))           { movTime = doc["mov_time"]; configChanged = true; }
  if (doc.containsKey("valor_noche"))        { ValorNoche = doc["valor_noche"]; configChanged = true; }
  if (doc.containsKey("histeresis_luz"))     { histeresisLuz = doc["histeresis_luz"]; configChanged = true; }
  if (doc.containsKey("sirena_time"))        { sirenaTime = doc["sirena_time"]; configChanged = true; }
  if (doc.containsKey("ref_time"))           { refTime = doc["ref_time"]; configChanged = true; }
  if (doc.containsKey("mov_rst"))            { movRst = doc["mov_rst"]; configChanged = true; }
  if (doc.containsKey("pir1_en"))            { PIR1En = doc["pir1_en"]; configChanged = true; }
  if (doc.containsKey("pir2_en"))            { PIR2En = doc["pir2_en"]; configChanged = true; }
  
  if (doc.containsKey("schedules")) {
    JsonArray schedArr = doc["schedules"].as<JsonArray>();
    int idx = 0;
    for (JsonObject obj : schedArr) {
      if (idx >= 8) break;
      schedules[idx].active = obj["active"] | false;
      schedules[idx].hour = obj["hour"] | 0;
      schedules[idx].minute = obj["minute"] | 0;
      schedules[idx].weekdays = obj["weekdays"] | 0;
      schedules[idx].target = obj["target"] | 0;
      schedules[idx].action = obj["action"] | 0;
      idx++;
    }
    for (int i = idx; i < 8; i++) {
      schedules[i].active = false;
    }
    configChanged = true;
  }
  
  if (configChanged) {
    saveConfig();
    actuadores();
  }
  
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// --- SETUP ---
void setup()
{
  // Cargar configuración de la EEPROM
  loadConfig();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, wifiPassword);
  // Inicializar NTP para Argentina (GMT-3)
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  // Conexión no bloqueante: se gestionará en el handleWiFi() de fondo

  ArduinoOTA.setHostname("Control_Frente");
  ArduinoOTA.setPassword("Aveni793");

  ArduinoOTA.onStart([]() 
  {
    digitalWrite(Reflectores, HIGH);
    digitalWrite(Sirena, HIGH);
    digitalWrite(LuzVereda, HIGH);
    digitalWrite(Camaras, HIGH);
    digitalWrite(SalidaBz, HIGH);
    delay(250);
    digitalWrite(SalidaBz, LOW); 
  });

  ArduinoOTA.onEnd([]()
  {
    digitalWrite(Reflectores, HIGH);
    digitalWrite(Sirena, HIGH);
    digitalWrite(LuzVereda, HIGH);
    digitalWrite(Camaras, HIGH);
    CuentaMov = 0;
    digitalWrite(SalidaBz, HIGH);
    delay(250);
    digitalWrite(SalidaBz, LOW);
    delay(500);
    digitalWrite(SalidaBz, HIGH);
    delay(250);
    digitalWrite(SalidaBz, LOW);
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {});

  ArduinoOTA.onError([](ota_error_t error) {});

  ArduinoOTA.begin();

  esp8266Client.setTimeout(1500); // 1.5 segundos de timeout para evitar congelamiento de WDT

  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(callback);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/config", HTTP_POST, handleApiConfig);

  pinMode(MovPIR1, INPUT);
  pinMode(MovPIR2, INPUT);
  pinMode(SalidaBz, OUTPUT);
  pinMode(Reflectores, OUTPUT);
  pinMode(Sirena, OUTPUT);
  pinMode(LuzVereda, OUTPUT);
  pinMode(Camaras, OUTPUT);

  // Inicializar actuadores al estado guardado en la carga de configuración
  actuadores();
  digitalWrite(SalidaBz, LOW);

  dht.begin();
  
  secondTicker.attach(1, onSecondTick);

  server.onNotFound(handleNotFound);
  server.begin();

  delay(2000);
}

void evaluarHorarios() {
  struct tm infoTiempo;
  if (!obtenerHoraLocal(infoTiempo)) {
    return; // Esperar a que NTP sincronice
  }

  // Ejecutar solo al cambiar de minuto
  if (infoTiempo.tm_min == ultimoMinutoEvaluado) {
    return;
  }
  ultimoMinutoEvaluado = infoTiempo.tm_min;

  int diaHoy = infoTiempo.tm_wday; // 0=Domingo, 1=Lunes, ..., 6=Sábado
  int horaHoy = infoTiempo.tm_hour;
  int minHoy = infoTiempo.tm_min;

  bool configChanged = false;

  for (int i = 0; i < 8; i++) {
    if (!schedules[i].active) continue;

    // Verificar si el día de hoy está en la máscara de bits
    if (!(schedules[i].weekdays & (1 << diaHoy))) continue;

    // Verificar si coincide la hora y minuto
    if (schedules[i].hour == horaHoy && schedules[i].minute == minHoy) {
      int target = schedules[i].target;
      int action = schedules[i].action;

      if (target == 0) { // Reflectores
        ReflectoresConf = action;
        configChanged = true;
        Serial.printf("Horario: ReflectoresConf cambiado a %d\n", action);
      } else if (target == 1) { // Luz Vereda
        LuzVeredaConf = action;
        configChanged = true;
        Serial.printf("Horario: LuzVeredaConf cambiado a %d\n", action);
      } else if (target == 2) { // Sirena
        SirenaConf = action;
        configChanged = true;
        Serial.printf("Horario: SirenaConf cambiado a %d\n", action);
      }
    }
  }

  if (configChanged) {
    saveConfig();
    actuadores();
    enviarDatos();
  }
}

// --- LOOP ---
void loop()
{
  ArduinoOTA.handle();
  evaluarHorarios();

  handleWiFi();
  handleMqtt();

  server.handleClient();

  digitalWrite(SalidaBz, LOW);

  if (safeRead(movCount) == 0 && safeRead(sirenaCount) == 0 && (!digitalRead(MovPIR1) || !PIR1En) && (!digitalRead(MovPIR2) || !PIR2En)) flagMov = false; 

  if (PIR1En && digitalRead(MovPIR1) && !flagMov)
  {
    flagMov = true;
    CuentaMov++;
    SensorN = 1;
    enviarDatos();
    alarma();
    digitalWrite(SalidaBz, HIGH);
    delay(250);
    digitalWrite(SalidaBz, LOW);
    SensorN = 0;
  }

  if (PIR2En && digitalRead(MovPIR2) && !flagMov)
  {
    flagMov = true;
    CuentaMov++;
    SensorN = 2;
    enviarDatos();
    alarma();
    digitalWrite(SalidaBz, HIGH);
    delay(250);
    digitalWrite(SalidaBz, LOW);
    SensorN = 0;
  }

  if ((safeRead(enviaDataCounter) >= intervalData) || Telemetria)
  {
    noInterrupts();
    enviaDataCounter = 0;
    interrupts();
    mediciones();
    enviarDatos();
  }

  valNocheHist = reflectoresST ? (ValorNoche - histeresisLuz) : (ValorNoche + histeresisLuz);

  actuadores();

  if (safeRead(movRstCount) == 0) CuentaMov = 0; 
}
