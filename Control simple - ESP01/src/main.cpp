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

#define MQTT_MAX_PACKET_SIZE 1024

// Definición de pines para ESP-01
#define CANAL1_PIN 0 // GPIO0 - Accionamiento de Reflector de Patio
#define CANAL2_PIN 2 // GPIO2 - Accionamiento de Luces Borde de Galería

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

struct Config {
  uint32_t signature;
  int canal1Conf;
  int canal2Conf;
  int intervalData;
} config;

void loadConfig() {
  EEPROM.begin(sizeof(Config));
  EEPROM.get(0, config);
  if (config.signature != EEPROM_SIGNATURE) {
    config.signature = EEPROM_SIGNATURE;
    config.canal1Conf = 0; // Apagado por defecto
    config.canal2Conf = 0; // Apagado por defecto
    config.intervalData = 60;
    
    EEPROM.put(0, config);
    EEPROM.commit();
  }
  
  canal1Conf = config.canal1Conf;
  canal2Conf = config.canal2Conf;
  intervalData = config.intervalData;
}

void saveConfig() {
  config.canal1Conf = canal1Conf;
  config.canal2Conf = canal2Conf;
  config.intervalData = intervalData;
  
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
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload_string);
    if (!error) {
      if (doc.containsKey("Luz")) {
        const char* luzStr = doc["Luz"];
        if (luzStr != nullptr) {
          String luzVal = String(luzStr);
          luzVal.toLowerCase();
          if (luzVal.indexOf("noche") >= 0) {
            esDeNoche = true;
          } else if (luzVal.indexOf("dia") >= 0 || luzVal.indexOf("d\u00eda") >= 0) {
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
    StaticJsonDocument<500> doc;
    DeserializationError error = deserializeJson(doc, payload_string);
    if (error) return;

    bool configChanged = false;

    if (doc.containsKey("canal1Conf"))   { canal1Conf = doc["canal1Conf"]; configChanged = true; }
    if (doc.containsKey("canal2Conf"))   { canal2Conf = doc["canal2Conf"]; configChanged = true; }
    if (doc.containsKey("intervalData")) { intervalData = doc["intervalData"]; configChanged = true; }
    if (doc.containsKey("Telemetria"))   { Telemetria = doc["Telemetria"]; }

    if (configChanged) {
      saveConfig();
      actuadores();
    }
  }
}

// --- ENVÍO DE DATOS TELEMETRÍA ---
void enviarDatos() {
  StaticJsonDocument<256> doc;

  doc["canal1ST"] = canal1ST;
  doc["canal1Conf"] = canal1Conf;
  doc["canal2ST"] = canal2ST;
  doc["canal2Conf"] = canal2Conf;
  doc["luzAmbiente"] = esDeNoche ? "noche" : "dia";
  doc["intervalData"] = intervalData;

  char buffer[256];
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

            } catch (e) {
                document.getElementById("conn-dot").className = "status-dot";
                document.getElementById("conn-text").innerText = "Desconectado";
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

// --- API WEB SERVER HANDLERS ---
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

void handleApiStatus() {
  StaticJsonDocument<256> doc;
  doc["canal1_st"] = canal1ST;
  doc["canal2_st"] = canal2ST;
  doc["canal1_conf"] = canal1Conf;
  doc["canal2_conf"] = canal2Conf;
  doc["luz_ambiente"] = esDeNoche ? "noche" : "dia";
  doc["interval_data"] = intervalData;

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
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    server.send(400, "application/json", "{\"error\":\"JSON Invalido\"}");
    return;
  }
  
  bool configChanged = false;
  
  if (doc.containsKey("canal1_conf"))   { canal1Conf = doc["canal1_conf"]; configChanged = true; }
  if (doc.containsKey("canal2_conf"))   { canal2Conf = doc["canal2_conf"]; configChanged = true; }
  if (doc.containsKey("interval_data")) { intervalData = doc["interval_data"]; configChanged = true; }
  
  if (configChanged) {
    saveConfig();
    actuadores();
  }
  
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// --- SETUP ---
void setup() {
  // Cargar configuración persistida
  loadConfig();

  // Asignar pines como salida (inicializar apagados/HIGH antes de configurar como salida para evitar destellos)
  digitalWrite(CANAL1_PIN, HIGH);
  digitalWrite(CANAL2_PIN, HIGH);
  pinMode(CANAL1_PIN, OUTPUT);
  pinMode(CANAL2_PIN, OUTPUT);

  // Inicializar estado físico según configuración cargada
  actuadores();

  // Conexión Wi-Fi
  WiFi.mode(WIFI_STA);
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

  secondTicker.attach(1, onSecondTick);

  delay(2000);
}

// --- LOOP ---
void loop() {
  ArduinoOTA.handle();

  handleWiFi();
  handleMqtt();

  server.handleClient();

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
