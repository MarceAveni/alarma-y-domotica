#include "conexiones.h"
#include <globales.h>
#include "sensores/sensores.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WebServer server(80);

// =======================
// CONEXIONES
// =======================

const char *ssid = "pocha";
const char *wifiPassword = "Vale07Marce05";
const char *mqttServer = "broker.emqx.io";   //"mqtt.eclipseprojects.io";
const int mqttPort = 1883;              // 1883 o 8084
const char *mqttUser = "Marce";        // Reemplaza con tu usuario MQTT
const char *mqttPassword = "Aveni793"; // Reemplaza con tu contraseña MQTT
const char *EspUser = "Marce";         // Usuario para accederal ESP32
const char *EspPassword = "EspAveni5"; // Contraseña para acceder al ESP32


WiFiEstado wifiEstado = WIFI_CONECTANDO;

uint8_t wifiIntentos = 0;
unsigned long ultimoIntentoWiFi = 0;
unsigned long ultimoOKWiFi = 0;

const uint8_t MAX_INTENTOS_WIFI = 10;
const unsigned long INTERVALO_INTENTO_WIFI = 5000;
const unsigned long TIEMPO_MAX_SIN_WIFI = 300000; // 5 minutos


// =======================
// CALLBACK MQTT
// =======================
void callback(char *topic, byte *payload, unsigned int length)
{
  char msg[length + 1];
  memcpy(msg, payload, length);
  msg[length] = '\0';

  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, msg)) return;

  procesarComandosJson(doc);
  enviarDatos();

  bool buzzerState = false;
  for(int i = 0; i < 1000; i++){ // hace un poco de ruido
    buzzerState = !buzzerState;
    digitalWrite(SalidaBz, !buzzerState);
    delayMicroseconds(100);
  }
  digitalWrite(SalidaBz, LOW);
}

// =======================
// MQTT RECONNECT
// =======================

void mqttConfig()
{
    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(callback);
}

unsigned long ultimoIntentoMQTT = 0;
const unsigned long INTERVALO_INTENTO_MQTT = 10000; // 10 segundos

void handleMqtt()
{
  if (WiFi.status() != WL_CONNECTED) return;

  if (!mqttClient.connected())
  {
    unsigned long ahora = millis();
    if (ahora - ultimoIntentoMQTT >= INTERVALO_INTENTO_MQTT)
    {
      ultimoIntentoMQTT = ahora;
      Serial.println("Intentando conectar a MQTT...");
      String clientId = "esp32Client_Patio_" + String((uint32_t)ESP.getEfuseMac(), HEX);
      if (mqttClient.connect(clientId.c_str(), mqttUser, mqttPassword))
      {
        Serial.println("MQTT Conectado");
        mqttClient.subscribe("BALH142N1788/Aveni793");
        
        digitalWrite(SalidaBz, HIGH);
        delay(100);
        digitalWrite(SalidaBz, LOW);
      }
      else
      {
        Serial.print("Fallo conexion MQTT, rc=");
        Serial.println(mqttClient.state());
      }
    }
  }
  else
  {
    mqttClient.loop();
  }
}

void wifiConect()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, wifiPassword);

  wifiEstado = WIFI_CONECTANDO;
  wifiIntentos = 0;
  ultimoIntentoWiFi = millis();

  Serial.println("WiFi: iniciando...");
}

void wifiLoop()
{
    wl_status_t st = WiFi.status();

    switch (wifiEstado)
    {
        // ===============================
        case WIFI_CONECTANDO:
        // ===============================
            if (st == WL_CONNECTED)
            {
                wifiEstado = WIFI_CONECTADO;
                wifiIntentos = 0;
                ultimoOKWiFi = millis();

                Serial.print("WiFi conectado. IP: ");
                Serial.println(WiFi.localIP());
            }
            else if (millis() - ultimoIntentoWiFi >= INTERVALO_INTENTO_WIFI)
            {
                ultimoIntentoWiFi = millis();
                wifiIntentos++;

                Serial.printf("WiFi intento %d/%d\n", wifiIntentos, MAX_INTENTOS_WIFI);

                if (wifiIntentos >= MAX_INTENTOS_WIFI)
                {
                    wifiEstado = WIFI_DESCONECTADO;
                    Serial.println("WiFi: sin conexión.");
                }
            }
            break;

        // ===============================
        case WIFI_CONECTADO:
        // ===============================
            if (st != WL_CONNECTED)
            {
                wifiEstado = WIFI_DESCONECTADO;
                ultimoIntentoWiFi = millis();
                Serial.println("WiFi desconectado.");
            }
            else
            {
                ultimoOKWiFi = millis(); // sigue vivo
            }
            break;

        // ===============================
        case WIFI_DESCONECTADO:
        // ===============================
            // Reintenta cada 10 segundos
            if (millis() - ultimoIntentoWiFi >= 10000)
            {
                Serial.println("WiFi: reintentando conexión...");
                WiFi.disconnect();
                WiFi.begin(ssid, wifiPassword);

                wifiEstado = WIFI_CONECTANDO;
                wifiIntentos = 0;
                ultimoIntentoWiFi = millis();
            }

            // Demasiado tiempo sin WiFi → reset
            if (millis() - ultimoOKWiFi > TIEMPO_MAX_SIN_WIFI)
            {
                Serial.println("WiFi muerto demasiado tiempo. Reiniciando...");
                delay(100);
                ESP.restart();
            }
            break;
    }
}


void otaConfig()
{
    ArduinoOTA.setHostname("Control_Patio");
    ArduinoOTA.setPassword(EspPassword);

    ArduinoOTA.onStart([]()
    {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        digitalWrite(bombaPileta, LOW);
        digitalWrite(bombaCisterna, HIGH);
        digitalWrite(bombaTanque, HIGH);
        digitalWrite(Reflectores, LOW);
        digitalWrite(luzPileta, HIGH);
        digitalWrite(luzGaleria, HIGH);
        digitalWrite(luzGaleriaBorde, HIGH);
        digitalWrite(salidaAux1, HIGH);
        digitalWrite(salidaAux2, HIGH);
        bool buzzerState = false;
        for(int i = 0; i < 5000; i++){ // hace un poco de ruido
          buzzerState = !buzzerState;
          digitalWrite(SalidaBz, !buzzerState);
          delayMicroseconds(250);
        }
        digitalWrite(SalidaBz, LOW);
    });

    ArduinoOTA.onEnd([]()
    {
        Serial.println("\nEnd");
        bool buzzerState = false;
        for(int i = 0; i < 5000; i++){ // hace un poco de ruido
          buzzerState = !buzzerState;
          digitalWrite(SalidaBz, !buzzerState);
          delayMicroseconds(250);
        }
        delay(500);
        for(int i = 0; i < 5000; i++){ // hace un poco de ruido
          buzzerState = !buzzerState;
          digitalWrite(SalidaBz, !buzzerState);
          delayMicroseconds(250);
        }
        digitalWrite(SalidaBz, LOW);
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {});

    ArduinoOTA.onError([](ota_error_t error)
    {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
        }
        
        bool buzzerState = false;
        for(int i = 0; i < 10000; i++){ // hace un poco de ruido
          buzzerState = !buzzerState;
          digitalWrite(SalidaBz, !buzzerState);
          delayMicroseconds(400);
        }
        digitalWrite(SalidaBz, LOW);
    });

    ArduinoOTA.begin();
}

void handleStatus()
{
  DynamicJsonDocument doc(2048);
  crearEstadoJson(doc);
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleControl()
{
  if (!server.hasArg("plain"))
  {
    server.send(400, "application/json", "{\"error\":\"Body vacio\"}");
    return;
  }

  String body = server.arg("plain");
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, body);
  if (error)
  {
    server.send(400, "application/json", "{\"error\":\"JSON Invalido\"}");
    return;
  }

  procesarComandosJson(doc);

  DynamicJsonDocument responseDoc(2048);
  crearEstadoJson(responseDoc);
  String response;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Control Patio - Local Dashboard</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-color: #0f172a;
            --card-bg: rgba(30, 41, 59, 0.7);
            --border-color: rgba(255, 255, 255, 0.08);
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
            --primary: #3b82f6;
            --primary-hover: #2563eb;
            --accent-cyan: #06b6d4;
            --accent-green: #10b981;
            --accent-red: #ef4444;
            --accent-orange: #f97316;
            --accent-yellow: #eab308;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: 'Inter', sans-serif;
            background-color: var(--bg-color);
            background-image: 
                radial-gradient(at 0% 0%, rgba(59, 130, 246, 0.1) 0px, transparent 50%),
                radial-gradient(at 100% 100%, rgba(6, 182, 212, 0.1) 0px, transparent 50%);
            color: var(--text-main);
            min-height: 100vh;
            padding: 20px;
            display: flex;
            flex-direction: column;
            align-items: center;
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
            font-size: 1.25rem;
            font-weight: 600;
            background: linear-gradient(135deg, #fff 0%, var(--text-muted) 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }
        .status-badge {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 0.85rem;
            color: var(--text-muted);
        }
        .status-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background-color: var(--accent-red);
            box-shadow: 0 0 8px var(--accent-red);
        }
        .status-dot.active {
            background-color: var(--accent-green);
            box-shadow: 0 0 8px var(--accent-green);
        }
        .container {
            width: 100%;
            max-width: 1000px;
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
            gap: 20px;
        }
        .card {
            background: var(--card-bg);
            backdrop-filter: blur(10px);
            border: 1px solid var(--border-color);
            border-radius: 20px;
            padding: 20px;
            display: flex;
            flex-direction: column;
            gap: 16px;
        }
        .card-title {
            font-size: 1rem;
            font-weight: 600;
            color: var(--text-main);
            border-bottom: 1px solid var(--border-color);
            padding-bottom: 8px;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .telemetry-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 0.9rem;
        }
        .telemetry-label { color: var(--text-muted); }
        .telemetry-value { font-weight: 600; }
        .control-group {
            display: flex;
            flex-direction: column;
            gap: 8px;
        }
        .control-label {
            font-size: 0.8rem;
            color: var(--text-muted);
            text-transform: uppercase;
            letter-spacing: 0.05em;
        }
        .radio-group {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 4px;
            background: rgba(0, 0, 0, 0.2);
            padding: 4px;
            border-radius: 10px;
            border: 1px solid var(--border-color);
        }
        .radio-group input[type="radio"] { display: none; }
        .radio-group label {
            text-align: center;
            padding: 6px;
            font-size: 0.8rem;
            font-weight: 500;
            color: var(--text-muted);
            cursor: pointer;
            border-radius: 8px;
            transition: all 0.2s;
        }
        .radio-group input[type="radio"]:checked + label {
            background: var(--primary);
            color: #fff;
            box-shadow: 0 2px 4px rgba(59, 130, 246, 0.4);
        }
        .switch-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .switch {
            position: relative;
            display: inline-block;
            width: 44px;
            height: 24px;
        }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider {
            position: absolute;
            cursor: pointer;
            top: 0; left: 0; right: 0; bottom: 0;
            background-color: rgba(255,255,255,0.1);
            transition: .3s;
            border-radius: 24px;
            border: 1px solid var(--border-color);
        }
        .slider:before {
            position: absolute;
            content: "";
            height: 16px; width: 16px;
            left: 3px; bottom: 3px;
            background-color: var(--text-muted);
            transition: .3s;
            border-radius: 50%;
        }
        input:checked + .slider { background-color: var(--primary); }
        input:checked + .slider:before {
            transform: translateX(20px);
            background-color: #fff;
        }
        .range-slider {
            display: flex;
            align-items: center;
            gap: 12px;
        }
        .range-slider input[type="range"] {
            flex: 1;
            height: 6px;
            background: rgba(255,255,255,0.1);
            border-radius: 3px;
            outline: none;
            -webkit-appearance: none;
        }
        .range-slider input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 16px; height: 16px;
            border-radius: 50%;
            background: var(--primary);
            cursor: pointer;
            box-shadow: 0 0 8px var(--primary);
        }
        .state-indicator {
            display: inline-flex;
            align-items: center;
            gap: 6px;
            font-size: 0.8rem;
            font-weight: 500;
        }
        .dot {
            width: 8px; height: 8px;
            border-radius: 50%;
            background-color: var(--text-muted);
        }
        .dot.active-cyan { background-color: var(--accent-cyan); box-shadow: 0 0 6px var(--accent-cyan); }
        .dot.active-yellow { background-color: var(--accent-yellow); box-shadow: 0 0 6px var(--accent-yellow); }
        .dot.active-green { background-color: var(--accent-green); box-shadow: 0 0 6px var(--accent-green); }
        
        .toast {
            position: fixed;
            bottom: 20px;
            background: var(--accent-green);
            color: #fff;
            padding: 10px 20px;
            border-radius: 10px;
            font-size: 0.85rem;
            font-weight: 500;
            box-shadow: 0 4px 12px rgba(16, 185, 129, 0.3);
            opacity: 0;
            transform: translateY(100px);
            transition: all 0.3s;
            pointer-events: none;
        }
        .toast.show { opacity: 1; transform: translateY(0); }
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
            background: var(--accent-cyan);
            color: #000;
            border-color: transparent;
            box-shadow: 0 0 6px var(--accent-cyan);
        }
    </style>
</head>
<body>
    <header>
        <h1>Control Patio - Local Dashboard</h1>
        <div class="status-badge">
            <div id="conn-dot" class="status-dot"></div>
            <span id="conn-text">Conectando...</span>
        </div>
    </header>

    <div class="container">
        <!-- Tarjeta Climatizacion -->
        <div class="card">
            <div class="card-title">
                <span>Climatización de Pileta</span>
                <div class="state-indicator">
                    <div id="st-bomba-pileta" class="dot"></div>
                    <span id="txt-bomba-pileta">Apagada</span>
                </div>
            </div>
            
            <div class="telemetry-row">
                <span class="telemetry-label">Temperatura Pileta:</span>
                <span id="val-temp-pileta" class="telemetry-value">--°C</span>
            </div>
            <div class="telemetry-row">
                <span class="telemetry-label">Temperatura Colector:</span>
                <span id="val-temp-colector" class="telemetry-value">--°C</span>
            </div>

            <div class="control-group">
                <span class="control-label">Modo Bomba Pileta</span>
                <div class="radio-group">
                    <input type="radio" name="bombaPileta" id="bp-0" value="0" onchange="sendConfig('bombaPiletaConf', 0)">
                    <label for="bp-0">OFF</label>
                    <input type="radio" name="bombaPileta" id="bp-1" value="1" onchange="sendConfig('bombaPiletaConf', 1)">
                    <label for="bp-1">ON</label>
                    <input type="radio" name="bombaPileta" id="bp-2" value="2" onchange="sendConfig('bombaPiletaConf', 2)">
                    <label for="bp-2">AUTO</label>
                </div>
            </div>

            <div class="control-group">
                <span class="control-label">Temperatura Consigna (<span id="val-set">--</span>°C)</span>
                <div class="range-slider">
                    <input type="range" id="slider-set" min="15" max="45" step="0.5" onchange="sendConfig('tempSet', parseFloat(this.value))" oninput="document.getElementById('val-set').innerText=this.value">
                </div>
            </div>

            <div class="switch-row">
                <span class="telemetry-label">Intercambiar Sensores</span>
                <label class="switch">
                    <input type="checkbox" id="ctrl-swap" onchange="sendConfig('IntercambioST', this.checked)">
                    <span class="slider"></span>
                </label>
            </div>
        </div>

        <!-- Tarjeta Bombas de Agua -->
        <div class="card">
            <div class="card-title">
                <span>Gestión de Bombas</span>
                <span id="val-nivel" style="font-size: 0.8rem; font-weight: 600;">--</span>
            </div>

            <div class="switch-row">
                <div style="display:flex; flex-direction:column;">
                    <span class="telemetry-value">Bomba Cisterna</span>
                    <span class="control-label" style="margin-top:2px;">Estado: <span id="txt-bomba-cisterna">Apagada</span></span>
                </div>
                <label class="switch">
                    <input type="checkbox" id="ctrl-bc" onchange="sendConfig('bombaCisternaConf', this.checked ? 1 : 0)">
                    <span class="slider"></span>
                </label>
            </div>

            <div class="switch-row">
                <div style="display:flex; flex-direction:column;">
                    <span class="telemetry-value">Bomba Tanque</span>
                    <span class="control-label" style="margin-top:2px;">Estado: <span id="txt-bomba-tanque">Apagada</span></span>
                </div>
                <label class="switch">
                    <input type="checkbox" id="ctrl-bt" onchange="sendConfig('bombaTanqueConf', this.checked ? 1 : 0)">
                    <span class="slider"></span>
                </label>
            </div>
        </div>

        <!-- Tarjeta Iluminacion -->
        <div class="card">
            <div class="card-title">
                <span>Iluminación Exterior</span>
                <span id="val-ldr" style="font-size: 0.8rem; font-weight: 500; color: var(--text-muted);">--</span>
            </div>

            <div class="control-group">
                <div class="switch-row" style="margin-bottom: 2px;">
                    <span class="telemetry-label">Reflectores Patio</span>
                    <span class="state-indicator"><div id="st-ref" class="dot"></div><span id="txt-ref">--</span></span>
                </div>
                <div class="radio-group">
                    <input type="radio" name="ref" id="ref-0" value="0" onchange="sendConfig('reflectoresConf', 0)">
                    <label for="ref-0">OFF</label>
                    <input type="radio" name="ref" id="ref-1" value="1" onchange="sendConfig('reflectoresConf', 1)">
                    <label for="ref-1">ON</label>
                    <input type="radio" name="ref" id="ref-2" value="2" onchange="sendConfig('reflectoresConf', 2)">
                    <label for="ref-2">AUTO</label>
                </div>
            </div>

            <div class="control-group">
                <div class="switch-row" style="margin-bottom: 2px;">
                    <span class="telemetry-label">Luces Pileta</span>
                    <span class="state-indicator"><div id="st-luz-p" class="dot"></div><span id="txt-luz-p">--</span></span>
                </div>
                <div class="radio-group">
                    <input type="radio" name="luzP" id="lp-0" value="0" onchange="sendConfig('luzPiletaConf', 0)">
                    <label for="lp-0">OFF</label>
                    <input type="radio" name="luzP" id="lp-1" value="1" onchange="sendConfig('luzPiletaConf', 1)">
                    <label for="lp-1">ON</label>
                    <input type="radio" name="luzP" id="lp-2" value="2" onchange="sendConfig('luzPiletaConf', 2)">
                    <label for="lp-2">AUTO</label>
                </div>
            </div>

            <div class="control-group">
                <div class="switch-row" style="margin-bottom: 2px;">
                    <span class="telemetry-label">Luz Galería</span>
                    <span class="state-indicator"><div id="st-gal" class="dot"></div><span id="txt-gal">--</span></span>
                </div>
                <div class="radio-group">
                    <input type="radio" name="gal" id="gal-0" value="0" onchange="sendConfig('luzGaleriaConf', 0)">
                    <label for="gal-0">OFF</label>
                    <input type="radio" name="gal" id="gal-1" value="1" onchange="sendConfig('luzGaleriaConf', 1)">
                    <label for="gal-1">ON</label>
                    <input type="radio" name="gal" id="gal-2" value="2" onchange="sendConfig('luzGaleriaConf', 2)">
                    <label for="gal-2">AUTO</label>
                </div>
            </div>

            <div class="control-group">
                <div class="switch-row" style="margin-bottom: 2px;">
                    <span class="telemetry-label">Borde Galería</span>
                    <span class="state-indicator"><div id="st-galb" class="dot"></div><span id="txt-galb">--</span></span>
                </div>
                <div class="radio-group">
                    <input type="radio" name="galB" id="galb-0" value="0" onchange="sendConfig('luzGaleriaBordeConf', 0)">
                    <label for="galb-0">OFF</label>
                    <input type="radio" name="galB" id="galb-1" value="1" onchange="sendConfig('luzGaleriaBordeConf', 1)">
                    <label for="galb-1">ON</label>
                    <input type="radio" name="galB" id="galb-2" value="2" onchange="sendConfig('luzGaleriaBordeConf', 2)">
                    <label for="galb-2">AUTO</label>
                </div>
            </div>
        </div>

        <!-- Tarjeta Reles Auxiliares y Sensores Techo -->
        <div class="card">
            <div class="card-title">Auxiliares y Techo</div>
            
            <div class="switch-row">
                <span class="telemetry-label">Relé Auxiliar 1</span>
                <label class="switch">
                    <input type="checkbox" id="ctrl-aux1" onchange="sendConfig('aux1En', this.checked)">
                    <span class="slider"></span>
                </label>
            </div>

            <div class="switch-row">
                <span class="telemetry-label">Relé Auxiliar 2</span>
                <label class="switch">
                    <input type="checkbox" id="ctrl-aux2" onchange="sendConfig('aux2En', this.checked)">
                    <span class="slider"></span>
                </label>
            </div>

            <div style="border-top:1px solid var(--border-color); padding-top:12px; margin-top:4px;">
                <span class="control-label" style="margin-bottom:8px; display:block;">Telemetría de Techo</span>
                <div class="telemetry-row">
                    <span class="telemetry-label">Temp Techo:</span>
                    <span id="val-temp-techo" class="telemetry-value">--°C</span>
                </div>
                <div class="telemetry-row" style="margin-top:6px;">
                    <span class="telemetry-label">Humedad Techo:</span>
                    <span id="val-hum-techo" class="telemetry-value">--%</span>
                </div>
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
                <div class="switch-row">
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
                        <option value="1">Luz Pileta</option>
                        <option value="2">Luz Galería</option>
                        <option value="3">Borde Galería</option>
                        <option value="4">Bomba Pileta</option>
                        <option value="5">Bomba Cisterna</option>
                        <option value="6">Bomba Tanque</option>
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

    <div id="toast" class="toast">Configuración aplicada</div>

    <script>
        function showToast() {
            const toast = document.getElementById("toast");
            toast.className = "toast show";
            setTimeout(() => { toast.className = "toast"; }, 2000);
        }

        async function pollStatus() {
            try {
                const response = await fetch('/status');
                const data = await response.json();
                
                document.getElementById("conn-dot").className = "status-dot active";
                document.getElementById("conn-text").innerText = "Conectado";

                // Climatizacion
                document.getElementById("val-temp-pileta").innerText = data.tempPileta.toFixed(1) + "°C (" + data.tempPiletaCrudo.toFixed(1) + "°C)";
                document.getElementById("val-temp-colector").innerText = data.tempColector.toFixed(1) + "°C (" + data.tempColectorCrudo.toFixed(1) + "°C)";
                document.getElementById("val-set").innerText = data.tempSet.toFixed(1);
                document.getElementById("slider-set").value = data.tempSet;
                
                const dotBP = document.getElementById("st-bomba-pileta");
                const txtBP = document.getElementById("txt-bomba-pileta");
                if(data.bombaPiletaST) {
                    dotBP.className = "dot active-cyan";
                    txtBP.innerText = "ENCENDIDA";
                    txtBP.style.color = "var(--accent-cyan)";
                } else {
                    dotBP.className = "dot";
                    txtBP.innerText = "Apagada";
                    txtBP.style.color = "var(--text-muted)";
                }
                document.getElementById("bp-" + data.bombaPiletaConf).checked = true;
                document.getElementById("ctrl-swap").checked = data.IntercambioST;

                // Bombas
                const lblNivel = document.getElementById("val-nivel");
                if (data.nivelAgua) {
                    lblNivel.innerText = "CISTERNA CON AGUA";
                    lblNivel.style.color = "var(--accent-green)";
                } else {
                    lblNivel.innerText = "CISTERNA VACÍA";
                    lblNivel.style.color = "var(--accent-red)";
                }
                
                document.getElementById("ctrl-bc").checked = (data.bombaCisternaConf === 1);
                const txtBC = document.getElementById("txt-bomba-cisterna");
                if (data.bombaCisternaST) {
                    txtBC.innerText = "ACCIONADA";
                    txtBC.style.color = "var(--accent-green)";
                } else {
                    txtBC.innerText = "Apagada";
                    txtBC.style.color = "var(--text-muted)";
                }

                document.getElementById("ctrl-bt").checked = (data.bombaTanqueConf === 1);
                const txtBT = document.getElementById("txt-bomba-tanque");
                if (data.bombaTanqueST) {
                    txtBT.innerText = "ACCIONADA";
                    txtBT.style.color = "var(--accent-green)";
                } else {
                    txtBT.innerText = "Apagada";
                    txtBT.style.color = "var(--text-muted)";
                }

                // Iluminacion
                document.getElementById("val-ldr").innerText = "LDR: " + data.luzAmbiente;
                
                // Reflectores
                const dotRef = document.getElementById("st-ref");
                const txtRef = document.getElementById("txt-ref");
                if (data.reflectoresST) {
                    dotRef.className = "dot active-cyan";
                    txtRef.innerText = "ENCENDIDOS";
                    txtRef.style.color = "var(--accent-cyan)";
                } else {
                    dotRef.className = "dot";
                    txtRef.innerText = "Apagados";
                    txtRef.style.color = "var(--text-muted)";
                }
                document.getElementById("ref-" + data.reflectoresConf).checked = true;

                // Luz Pileta
                const dotLuzP = document.getElementById("st-luz-p");
                const txtLuzP = document.getElementById("txt-luz-p");
                if (data.luzPiletaST) {
                    dotLuzP.className = "dot active-cyan";
                    txtLuzP.innerText = "ENCENDIDA";
                    txtLuzP.style.color = "var(--accent-cyan)";
                } else {
                    dotLuzP.className = "dot";
                    txtLuzP.innerText = "Apagada";
                    txtLuzP.style.color = "var(--text-muted)";
                }
                document.getElementById("lp-" + data.luzPiletaConf).checked = true;

                // Luz Galeria
                const dotGal = document.getElementById("st-gal");
                const txtGal = document.getElementById("txt-gal");
                if (data.luzGaleriaST) {
                    dotGal.className = "dot active-yellow";
                    txtGal.innerText = "ENCENDIDA";
                    txtGal.style.color = "var(--accent-yellow)";
                } else {
                    dotGal.className = "dot";
                    txtGal.innerText = "Apagada";
                    txtGal.style.color = "var(--text-muted)";
                }
                document.getElementById("gal-" + data.luzGaleriaConf).checked = true;

                // Luz Galeria Borde
                const dotGalB = document.getElementById("st-galb");
                const txtGalB = document.getElementById("txt-galb");
                if (data.luzGaleriaBordeST) {
                    dotGalB.className = "dot active-yellow";
                    txtGalB.innerText = "ENCENDIDA";
                    txtGalB.style.color = "var(--accent-yellow)";
                } else {
                    dotGalB.className = "dot";
                    txtGalB.innerText = "Apagada";
                    txtGalB.style.color = "var(--text-muted)";
                }
                document.getElementById("galb-" + data.luzGaleriaBordeConf).checked = true;

                // Auxiliares
                document.getElementById("ctrl-aux1").checked = data.aux1En;
                document.getElementById("ctrl-aux2").checked = data.aux2En;

                // Techo
                document.getElementById("val-temp-techo").innerText = data.tempTecho.toFixed(1) + "°C";
                document.getElementById("val-hum-techo").innerText = data.humTecho.toFixed(0) + "%";

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
                const response = await fetch('/control', {
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
                const response = await fetch('/control', {
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

void handleRoot()
{
  server.send_P(200, "text/html", INDEX_HTML);
}

void serverConfig()
{
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/control", HTTP_POST, handleControl);
  server.onNotFound([]() {
    server.send(404, "text/plain", "404: Not Found");
  });
  server.begin();
  Serial.println("Servidor HTTP local iniciado en puerto 80");
}