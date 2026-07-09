#define MQTT_MAX_PACKET_SIZE 1024

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <time.h>


// Definición de pines para ESP-01
#define CANAL1_PIN 2 // GPIO2 - Accionamiento de Reflector de Patio
#define CANAL2_PIN 0 // GPIO0 - Accionamiento de Luces Borde de Galería

#define BOTON1_PIN 3 // GPIO3 (RX) - Botón Canal 1 (Reflector Patio)
#define BOTON2_PIN 1 // GPIO1 (TX) - Botón Canal 2 (Borde Galería)

#define TOUCH_ACTIVE_STATE LOW // LOW para sensores capacitivos activos en bajo (TTP223 con puente A cerrado)


// Prototipos de funciones
void actuadores();
void enviarDatos();
void saveConfig();

// Definición de variables y constantes
int intervalData = 60;                // Intervalo de tiempo [seg] en el que se envían datos
bool Telemetria = false;              // Flag para forzar el envío de telemetría completa
const char *ssid = "pocha";
const char *wifiPassword = "Vale07Marce05";
const char *mqttServer = "broker.emqx.io";
const int mqttPort = 1883;
const char *mqttUser = "Marce";
const char *mqttPassword = "Aveni793";

int canal1Conf = 0;                   // 0 = Siempre apagado, 1 = Siempre encendido, 2 = Automático
bool canal1ST = false;                // Estado físico actual del Canal 1 (true = encendido)
int canal2Conf = 0;                   // 0 = Siempre apagado, 1 = Siempre encendido, 2 = Automático
bool canal2ST = false;                // Estado físico actual del Canal 2 (true = encendido)

bool esDeNoche = false;               // Determina si es de noche (suscrito desde Frente)
volatile int enviaDataCounter = 0;

Ticker secondTicker;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
ESP8266WebServer server(80);          // Servidor HTTP local en el puerto 80

// --- PERSISTENCIA EEPROM ---
const uint32_t EEPROM_SIGNATURE = 0x55AA8899;

struct ScheduleRule {
  bool active;
  uint8_t hour;
  uint8_t minute;
  uint8_t weekdays;  // Bit 0 = Dom, Bit 1 = Lun, ..., Bit 6 = Sab
  uint8_t target;    // 0: Canal 1, 1: Canal 2
  uint8_t action;    // 0: Apagado, 1: Encendido, 2: Auto
};

ScheduleRule schedules[8];
int ultimoMinutoEvaluado = -1;

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
  int canal1Conf;
  int canal2Conf;
  int intervalData;
  ScheduleRule schedules[8];
} config;

void loadConfig() {
  EEPROM.begin(sizeof(Config));
  EEPROM.get(0, config);
  if (config.signature != EEPROM_SIGNATURE) {
    config.signature = EEPROM_SIGNATURE;
    config.canal1Conf = 0; // Apagado por defecto
    config.canal2Conf = 0; // Apagado por defecto
    config.intervalData = 60;
    
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
  
  canal1Conf = config.canal1Conf;
  canal2Conf = config.canal2Conf;
  intervalData = config.intervalData;
  for (int i = 0; i < 8; i++) {
    schedules[i] = config.schedules[i];
  }
}

void saveConfig() {
  config.canal1Conf = canal1Conf;
  config.canal2Conf = canal2Conf;
  config.intervalData = intervalData;
  for (int i = 0; i < 8; i++) {
    config.schedules[i] = schedules[i];
  }
  
  EEPROM.put(0, config);
  EEPROM.commit();
}

// --- ACTUALIZACIÓN DE ESTADOS FÍSICOS (ACTUADORES) ---
void actuadores() {
  // Canal 1: Reflector Patio (Relé Optoacoplado - Active LOW)
  if (canal1Conf == 1 || (canal1Conf == 2 && esDeNoche)) {
    digitalWrite(CANAL1_PIN, LOW); // Enciende
    canal1ST = true;
  } else {
    digitalWrite(CANAL1_PIN, HIGH); // Apaga
    canal1ST = false;
  }

  // Canal 2: Borde de Galería (Relé Optoacoplado - Active LOW)
  if (canal2Conf == 1 || (canal2Conf == 2 && esDeNoche)) {
    digitalWrite(CANAL2_PIN, LOW); // Enciende
    canal2ST = true;
  } else {
    digitalWrite(CANAL2_PIN, HIGH); // Apaga
    canal2ST = false;
  }
}

// --- CALLBACK MQTT ---
void callback(char *topic, byte *payload, unsigned int length) {
  char payload_string[length + 1];
  memcpy(payload_string, payload, length);
  payload_string[length] = '\0';

  // 1. Suscripción a la luz ambiente del Frente
  if (strcmp(topic, "BALH142N1788/Frente") == 0) {
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload_string);
    if (!error) {
      if (doc.containsKey("Luz")) {
        const char* luzStr = doc["Luz"];
        if (luzStr != nullptr) {
          String luzVal = String(luzStr);
          luzVal.toLowerCase();
          if (luzVal.indexOf("noche") >= 0) {
            esDeNoche = true;
          } else {
            esDeNoche = false;
          }
          // Forzar actualización inmediata tras recibir cambio de luz ambiente
          actuadores();
        }
      }
    }
    return;
  }

  // 2. Suscripción a comandos generales
  if (strcmp(topic, "BALH142N1788/Aveni793") == 0) {
    DynamicJsonDocument doc(1500);
    DeserializationError error = deserializeJson(doc, payload_string);
    if (error) return;

    bool configChanged = false;

    if (doc.containsKey("canal1Conf"))   { canal1Conf = doc["canal1Conf"]; configChanged = true; }
    if (doc.containsKey("canal2Conf"))   { canal2Conf = doc["canal2Conf"]; configChanged = true; }
    if (doc.containsKey("intervalData")) { intervalData = doc["intervalData"]; configChanged = true; }
    if (doc.containsKey("Telemetria"))   { Telemetria = doc["Telemetria"]; }

    if (doc.containsKey("esp01Schedules") || doc.containsKey("schedules")) {
      JsonArray schedArr = doc.containsKey("esp01Schedules") ? doc["esp01Schedules"].as<JsonArray>() : doc["schedules"].as<JsonArray>();
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
      enviarDatos();
    }
  }
}

// --- ENVÍO DE DATOS TELEMETRÍA ---
void enviarDatos() {
  DynamicJsonDocument doc(1500);

  doc["canal1ST"] = canal1ST;
  doc["canal1Conf"] = canal1Conf;
  doc["canal2ST"] = canal2ST;
  doc["canal2Conf"] = canal2Conf;
  doc["luzAmbiente"] = esDeNoche ? "noche" : "dia";
  doc["intervalData"] = intervalData;

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

  char buffer[1000];
  size_t n = serializeJson(doc, buffer, sizeof(buffer));

  mqttClient.publish("BALH142N1788/ESP01", buffer, n);
  Telemetria = false;
}

void onSecondTick() {
  noInterrupts();
  enviaDataCounter++;
  interrupts();
}

// --- GESTIÓN WIFI Y MQTT NO BLOQUEANTE ---
unsigned long lastWiFiCheck = 0;
unsigned long wifiDisconnectTime = 0;

void handleWiFi() {
  unsigned long now = millis();
  if (now - lastWiFiCheck >= 1000) {
    lastWiFiCheck = now;
    if (WiFi.status() == WL_CONNECTED) {
      wifiDisconnectTime = 0;
    } else {
      if (wifiDisconnectTime == 0) {
        wifiDisconnectTime = now;
      } else if (now - wifiDisconnectTime > 300000) { // 5 minutos sin Wifi -> reinicia
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
  sprintf(clientBuf, "esp8266_ESP01_%02x%02x%02x", mac[3], mac[4], mac[5]);
  mqttClientId = String(clientBuf);
}

void handleMqtt() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttRetry >= 10000) { // Reintento cada 10 segundos
      lastMqttRetry = now;
      if (mqttClientId == "") {
        generateMqttClientId();
      }
      if (mqttClient.connect(mqttClientId.c_str(), mqttUser, mqttPassword)) {
        mqttClient.subscribe("BALH142N1788/Aveni793");
        mqttClient.subscribe("BALH142N1788/Frente"); // Para suscribirse a la luz ambiente
      }
    }
  } else {
    mqttClient.loop();
  }
}

// --- CONTENIDO HTML DEL DASHBOARD ---
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
    <meta name="description" content="Panel de Control Simple ESP-01">
    <title>Control ESP-01 - Local Dashboard</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-color: #0b0f19;
            --card-bg: rgba(22, 30, 49, 0.7);
            --border-color: rgba(255, 255, 255, 0.06);
            --text-main: #f1f5f9;
            --text-muted: #64748b;
            --primary: #4f46e5;
            --primary-hover: #4338ca;
            --accent-cyan: #06b6d4;
            --accent-orange: #f97316;
            --accent-green: #10b981;
            --accent-red: #ef4444;
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
                radial-gradient(at 0% 0%, rgba(79, 70, 229, 0.08) 0px, transparent 50%),
                radial-gradient(at 100% 100%, rgba(6, 182, 212, 0.08) 0px, transparent 50%);
            color: var(--text-main);
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 20px;
        }
 
        header {
            width: 100%;
            max-width: 800px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 24px;
            padding: 16px 24px;
            background: var(--card-bg);
            backdrop-filter: blur(12px);
            border: 1px solid var(--border-color);
            border-radius: 16px;
        }
 
        h1 {
            font-size: 1.25rem;
            font-weight: 600;
            letter-spacing: -0.5px;
            background: linear-gradient(135deg, #ffffff 0%, #cbd5e1 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }
 
        .status-badge {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 0.8rem;
            font-weight: 500;
            color: var(--text-muted);
            background: rgba(0, 0, 0, 0.3);
            padding: 6px 12px;
            border-radius: 9999px;
            border: 1px solid rgba(255, 255, 255, 0.03);
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
            max-width: 800px;
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(340px, 1fr));
            gap: 20px;
            margin-bottom: 24px;
        }
 
        .card {
            background: var(--card-bg);
            backdrop-filter: blur(12px);
            border: 1px solid var(--border-color);
            border-radius: 20px;
            padding: 24px;
            box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.4);
            display: flex;
            flex-direction: column;
            gap: 20px;
        }
 
        .card-title {
            font-size: 1rem;
            font-weight: 600;
            color: #ffffff;
            border-bottom: 1px solid rgba(255, 255, 255, 0.05);
            padding-bottom: 12px;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }
 
        .row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 8px 0;
        }
 
        .label {
            color: #94a3b8;
            font-size: 0.9rem;
            font-weight: 400;
        }
 
        .value {
            font-weight: 600;
            font-size: 0.95rem;
            color: #ffffff;
        }
 
        /* Segmented control for OFF/ON/AUTO */
        .segmented-control {
            display: flex;
            background: #0f172a;
            border-radius: 10px;
            padding: 2px;
            border: 1px solid rgba(255, 255, 255, 0.04);
            margin-top: 6px;
        }
 
        .segmented-control input {
            display: none;
        }
 
        .segmented-control label {
            flex: 1;
            text-align: center;
            padding: 8px 0;
            font-size: 0.75rem;
            font-weight: 600;
            color: var(--text-muted);
            cursor: pointer;
            border-radius: 8px;
            transition: all 0.2s ease;
            user-select: none;
        }
 
        .segmented-control input:checked + label {
            background: var(--primary);
            color: #ffffff;
            box-shadow: 0 4px 10px rgba(79, 70, 229, 0.3);
        }
 
        /* Indicator dots */
        .indicator {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 0.85rem;
            font-weight: 600;
        }
 
        .indicator-dot {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background-color: #334155;
            transition: all 0.2s;
        }
 
        .indicator-dot.active-cyan {
            background-color: var(--accent-cyan);
            box-shadow: 0 0 10px var(--accent-cyan);
        }
 
        .indicator-dot.active-orange {
            background-color: var(--accent-orange);
            box-shadow: 0 0 10px var(--accent-orange);
        }
 
        /* Inputs */
        .input-group {
            display: grid;
            grid-template-columns: 2fr 1fr;
            align-items: center;
            gap: 12px;
        }
 
        .input-group input {
            background: #0f172a;
            border: 1px solid var(--border-color);
            border-radius: 8px;
            color: #ffffff;
            padding: 8px 12px;
            font-size: 0.85rem;
            text-align: right;
            width: 100%;
            outline: none;
            transition: border-color 0.2s;
        }
 
        .input-group input:focus {
            border-color: var(--primary);
        }
 
        #toast {
            visibility: hidden;
            min-width: 200px;
            background-color: rgba(15, 23, 42, 0.95);
            border: 1px solid var(--accent-green);
            color: #ffffff;
            text-align: center;
            border-radius: 8px;
            padding: 12px;
            position: fixed;
            z-index: 1000;
            bottom: 30px;
            box-shadow: 0 10px 20px rgba(0, 0, 0, 0.5);
            backdrop-filter: blur(8px);
            opacity: 0;
            transition: opacity 0.3s, visibility 0.3s;
            font-size: 0.85rem;
            font-weight: 500;
        }
 
        #toast.show {
            visibility: visible;
            opacity: 1;
        }
        
        footer {
            margin-top: auto;
            color: var(--text-muted);
            font-size: 0.75rem;
            padding: 20px 0;
            text-align: center;
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
            color: #ffffff;
            border-color: transparent;
            box-shadow: 0 0 6px var(--primary);
        }
    </style>
</head>
<body>
    <header>
        <h1>Control Simple ESP-01</h1>
        <div class="status-badge">
            <span id="conn-dot" class="status-dot"></span>
            <span id="conn-text">Desconectado</span>
        </div>
    </header>

    <div class="grid">
        <!-- Card: Actuadores & Sensores -->
        <div class="card">
            <div class="card-title">
                <span>Estado actual</span>
            </div>
            <div class="row">
                <span class="label">Canal 1: Reflector Patio</span>
                <div class="indicator">
                    <span id="st-c1" class="indicator-dot"></span>
                    <span id="txt-c1" style="color: var(--text-muted)">Apagado</span>
                </div>
            </div>
            <div class="row">
                <span class="label">Canal 2: Luz Borde Galería</span>
                <div class="indicator">
                    <span id="st-c2" class="indicator-dot"></span>
                    <span id="txt-c2" style="color: var(--text-muted)">Apagado</span>
                </div>
            </div>
            <div class="row">
                <span class="label">Luz ambiente (MQTT Frente)</span>
                <span id="val-luz" class="value">--</span>
            </div>
        </div>

        <!-- Card: Controles -->
        <div class="card">
            <div class="card-title">
                <span>Modos de Operación</span>
            </div>
            
            <div style="display: flex; flex-direction: column; gap: 4px;">
                <span class="label">Reflector Patio (Canal 1)</span>
                <div class="segmented-control">
                    <input type="radio" id="c1-0" name="c1_mode" value="0" onchange="sendConfig('canal1_conf', 0)">
                    <label for="c1-0">OFF</label>
                    <input type="radio" id="c1-1" name="c1_mode" value="1" onchange="sendConfig('canal1_conf', 1)">
                    <label for="c1-1">ON</label>
                    <input type="radio" id="c1-2" name="c1_mode" value="2" onchange="sendConfig('canal1_conf', 2)">
                    <label for="c1-2">AUTO</label>
                </div>
            </div>

            <div style="display: flex; flex-direction: column; gap: 4px; margin-top: 10px;">
                <span class="label">Luces Borde Galería (Canal 2)</span>
                <div class="segmented-control">
                    <input type="radio" id="c2-0" name="c2_mode" value="0" onchange="sendConfig('canal2_conf', 0)">
                    <label for="c2-0">OFF</label>
                    <input type="radio" id="c2-1" name="c2_mode" value="1" onchange="sendConfig('canal2_conf', 1)">
                    <label for="c2-1">ON</label>
                    <input type="radio" id="c2-2" name="c2_mode" value="2" onchange="sendConfig('canal2_conf', 2)">
                    <label for="c2-2">AUTO</label>
                </div>
            </div>
        </div>
        
        <!-- Card: Tiempos y Configuración -->
        <div class="card" style="grid-column: 1 / -1;">
            <div class="card-title">
                <span>Configuración de Red</span>
            </div>
            <div class="input-group">
                <label for="set-int" class="label">Intervalo de Telemetría (seg)</label>
                <input type="number" id="set-int" onblur="sendConfig('interval_data', parseInt(this.value))">
            </div>
        </div>

        <!-- Tarjeta Temporizadores Horarios -->
        <div class="card" style="grid-column: 1 / -1;">
            <div class="card-title">
                <span>Programación Horaria (Temporizadores)</span>
            </div>
            <div style="display: flex; gap: 12px; align-items: center; margin-bottom: 8px;">
                <span class="label">Seleccionar Regla:</span>
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
                <div class="row">
                    <span class="label">Regla Activa</span>
                    <label class="switch">
                        <input type="checkbox" id="regla-active" onchange="actualizarReglaLocal()">
                        <span class="slider"></span>
                    </label>
                </div>
                
                <div class="row">
                    <span class="label">Hora de Ejecución:</span>
                    <div style="display: flex; gap: 4px; align-items: center;">
                        <input type="number" id="regla-hora" min="0" max="23" placeholder="HH" onchange="actualizarReglaLocal()" style="width: 60px; text-align: center; background: rgba(0,0,0,0.4); color: #fff; border: 1px solid var(--border-color); padding: 4px; border-radius: 6px;">
                        <span style="color: var(--text-muted)">:</span>
                        <input type="number" id="regla-minuto" min="0" max="59" placeholder="MM" onchange="actualizarReglaLocal()" style="width: 60px; text-align: center; background: rgba(0,0,0,0.4); color: #fff; border: 1px solid var(--border-color); padding: 4px; border-radius: 6px;">
                    </div>
                </div>

                <div class="row">
                    <span class="label">Dispositivo Destino:</span>
                    <select id="regla-target" onchange="actualizarReglaLocal()" style="background: rgba(0,0,0,0.4); color: #fff; border: 1px solid var(--border-color); padding: 6px; border-radius: 6px; font-family: inherit;">
                        <option value="0">Reflector Patio</option>
                        <option value="1">Borde Galería</option>
                    </select>
                </div>

                <div class="row">
                    <span class="label">Modo / Acción:</span>
                    <select id="regla-action" onchange="actualizarReglaLocal()" style="background: rgba(0,0,0,0.4); color: #fff; border: 1px solid var(--border-color); padding: 6px; border-radius: 6px; font-family: inherit;">
                        <option value="0">Apagado (OFF)</option>
                        <option value="1">Encendido (ON)</option>
                        <option value="2">Automático (AUTO)</option>
                    </select>
                </div>

                <div style="display: flex; flex-direction: column; gap: 6px;">
                    <span class="label">Días Activos</span>
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
        Alarma y Domótica Residencial - ESP-01
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
                
                // Actualizar estado conexión
                document.getElementById("conn-dot").className = "status-dot online";
                document.getElementById("conn-text").innerText = "Conectado";

                // Canal 1 UI
                const stC1 = document.getElementById("st-c1");
                const txtC1 = document.getElementById("txt-c1");
                if (data.canal1_st) {
                    stC1.className = "indicator-dot active-cyan";
                    txtC1.innerText = "ENCENDIDO";
                    txtC1.style.color = "var(--accent-cyan)";
                } else {
                    stC1.className = "indicator-dot";
                    txtC1.innerText = "Apagado";
                    txtC1.style.color = "var(--text-muted)";
                }

                // Canal 2 UI
                const stC2 = document.getElementById("st-c2");
                const txtC2 = document.getElementById("txt-c2");
                if (data.canal2_st) {
                    stC2.className = "indicator-dot active-orange";
                    txtC2.innerText = "ENCENDIDO";
                    txtC2.style.color = "var(--accent-orange)";
                } else {
                    stC2.className = "indicator-dot";
                    txtC2.innerText = "Apagado";
                    txtC2.style.color = "var(--text-muted)";
                }

                // Luz ambiente
                const luzTxt = data.luz_ambiente === "noche" ? "Noche (Oscuro)" : "Día (Claro)";
                document.getElementById("val-luz").innerText = luzTxt;
                document.getElementById("val-luz").style.color = data.luz_ambiente === "noche" ? "var(--accent-cyan)" : "#fbbf24";

                // Configs (radios)
                document.getElementById("c1-" + data.canal1_conf).checked = true;
                document.getElementById("c2-" + data.canal2_conf).checked = true;

                // Inputs
                const intEl = document.getElementById("set-int");
                if (document.activeElement !== intEl) {
                    intEl.value = data.interval_data;
                }

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
                console.error("Error al actualizar config");
            }
        }

        // Iniciar polling
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
  doc["canal1_st"] = canal1ST;
  doc["canal2_st"] = canal2ST;
  doc["canal1_conf"] = canal1Conf;
  doc["canal2_conf"] = canal2Conf;
  doc["luz_ambiente"] = esDeNoche ? "noche" : "dia";
  doc["interval_data"] = intervalData;

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
  
  if (doc.containsKey("canal1_conf"))   { canal1Conf = doc["canal1_conf"]; configChanged = true; }
  if (doc.containsKey("canal2_conf"))   { canal2Conf = doc["canal2_conf"]; configChanged = true; }
  if (doc.containsKey("interval_data")) { intervalData = doc["interval_data"]; configChanged = true; }
  
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

// --- CONTROL DE BOTONES TOUCH MANUALES ---
void readButtons() {
  unsigned long now = millis();

  // --- BOTON 1 (Reflector Patio - Pin RX / GPIO3) ---
  bool b1Reading = (digitalRead(BOTON1_PIN) == TOUCH_ACTIVE_STATE);
  static bool b1LastState = false;
  static bool b1State = false;
  static unsigned long b1LastDebounceTime = 0;
  static unsigned long b1PressStartTime = 0;
  static bool b1LongPressHandled = false;

  if (b1Reading != b1LastState) {
    b1LastDebounceTime = now;
    b1LastState = b1Reading;
  }

  if ((now - b1LastDebounceTime) > 50) {
    if (b1Reading != b1State) {
      b1State = b1Reading;
      if (b1State) {
        // Recién presionado
        b1PressStartTime = now;
        b1LongPressHandled = false;
      } else {
        // Recién soltado
        if (!b1LongPressHandled && b1PressStartTime > 0 && (now - b1PressStartTime < 3000)) {
          // Toque corto: Conmutar encendido/apagado manual
          if (canal1ST) {
            canal1Conf = 0; // Apagar
          } else {
            canal1Conf = 1; // Encender
          }
          saveConfig();
          actuadores();
          enviarDatos();
        }
        b1PressStartTime = 0;
      }
    }
  }

  // Si está presionado, evaluar pulsación larga de 3 segundos
  if (b1State && !b1LongPressHandled) {
    if (now - b1PressStartTime >= 3000) {
      b1LongPressHandled = true;
      canal1Conf = 2; // Pasar a AUTOMÁTICO
      saveConfig();
      actuadores();
      enviarDatos();
    }
  }

  // --- BOTON 2 (Borde Galería - Pin TX / GPIO1) ---
  bool b2Reading = (digitalRead(BOTON2_PIN) == TOUCH_ACTIVE_STATE);
  static bool b2LastState = false;
  static bool b2State = false;
  static unsigned long b2LastDebounceTime = 0;
  static unsigned long b2PressStartTime = 0;
  static bool b2LongPressHandled = false;

  if (b2Reading != b2LastState) {
    b2LastDebounceTime = now;
    b2LastState = b2Reading;
  }

  if ((now - b2LastDebounceTime) > 50) {
    if (b2Reading != b2State) {
      b2State = b2Reading;
      if (b2State) {
        // Recién presionado
        b2PressStartTime = now;
        b2LongPressHandled = false;
      } else {
        // Recién soltado
        if (!b2LongPressHandled && b2PressStartTime > 0 && (now - b2PressStartTime < 3000)) {
          // Toque corto: Conmutar encendido/apagado manual
          if (canal2ST) {
            canal2Conf = 0; // Apagar
          } else {
            canal2Conf = 1; // Encender
          }
          saveConfig();
          actuadores();
          enviarDatos();
        }
        b2PressStartTime = 0;
      }
    }
  }

  // Si está presionado, evaluar pulsación larga de 3 segundos
  if (b2State && !b2LongPressHandled) {
    if (now - b2PressStartTime >= 3000) {
      b2LongPressHandled = true;
      canal2Conf = 2; // Pasar a AUTOMÁTICO
      saveConfig();
      actuadores();
      enviarDatos();
    }
  }
}

// --- SETUP ---
void setup() {
  // Asegurar que el puerto Serie esté apagado para liberar pines RX (GPIO3) y TX (GPIO1) como GPIO
  Serial.end();

  // Cargar configuración persistida
  loadConfig();

  // Asignar pines como salida (inicializar apagados/HIGH antes de configurar como salida para evitar destellos)
  digitalWrite(CANAL1_PIN, HIGH);
  digitalWrite(CANAL2_PIN, HIGH);
  pinMode(CANAL1_PIN, OUTPUT);
  pinMode(CANAL2_PIN, OUTPUT);

  // Configurar pines de botones touch como entrada
  pinMode(BOTON1_PIN, INPUT);
  pinMode(BOTON2_PIN, INPUT);

  // Inicializar estado físico según configuración cargada
  actuadores();

  // Conexión Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, wifiPassword);

  // Configuración de OTA
  ArduinoOTA.setHostname("Control_ESP01");
  ArduinoOTA.setPassword("Aveni793");

  ArduinoOTA.onStart([]() {
    // Apagar relés antes de escribir firmware
    digitalWrite(CANAL1_PIN, HIGH);
    digitalWrite(CANAL2_PIN, HIGH);
  });

  ArduinoOTA.onEnd([]() {
    digitalWrite(CANAL1_PIN, HIGH);
    digitalWrite(CANAL2_PIN, HIGH);
  });

  ArduinoOTA.begin();

  // Configurar timeout de cliente para evitar WDT resets por bloqueos de red
  espClient.setTimeout(1500);

  // Configurar servidor MQTT
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(callback);

  // Rutas de API y Web Local
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/config", HTTP_POST, handleApiConfig);
  server.onNotFound(handleNotFound);
  server.begin();

  // Inicializar NTP para Argentina (GMT-3)
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  secondTicker.attach(1, onSecondTick);

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

      if (target == 0) { // Canal 1
        canal1Conf = action;
        configChanged = true;
      } else if (target == 1) { // Canal 2
        canal2Conf = action;
        configChanged = true;
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
void loop() {
  ArduinoOTA.handle();
  evaluarHorarios();

  handleWiFi();
  handleMqtt();

  server.handleClient();

  // Leer estado de botones touch capacitivos
  readButtons();

  // Envío periódico de datos
  if ((enviaDataCounter >= intervalData) || Telemetria) {
    noInterrupts();
    enviaDataCounter = 0;
    interrupts();
    enviarDatos();
  }

  // Refrescar el estado de los actuadores regularmente
  actuadores();

  yield(); // Permitir operaciones en segundo plano de la pila Wifi del ESP8266
}
