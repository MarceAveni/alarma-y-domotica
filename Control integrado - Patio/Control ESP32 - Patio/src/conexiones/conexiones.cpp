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

  StaticJsonDocument<500> doc;
  if (deserializeJson(doc, msg)) return;

  if (doc.containsKey("intervalData")) intervalData = doc["intervalData"];
  if (doc.containsKey("tempDif_on")) tempDif_on = doc["tempDif_on"];
  if (doc.containsKey("tempDif_off")) tempDif_off = doc["tempDif_off"];
  if (doc.containsKey("tempSet")) tempSet = doc["tempSet"];
  if (doc.containsKey("minOnTime")) minOnTime = doc["minOnTime"];
  if (doc.containsKey("minOffTime")) minOffTime = doc["minOffTime"];
  if (doc.containsKey("maxOnTime")) maxOnTime = doc["maxOnTime"];
  if (doc.containsKey("bombaPiletaConf")) bombaPiletaConf = doc["bombaPiletaConf"];
  if (doc.containsKey("bombaCisternaConf")) bombaCisternaConf = doc["bombaCisternaConf"];
  if (doc.containsKey("bombaTanqueConf")) bombaTanqueConf = doc["bombaTanqueConf"];
  if (doc.containsKey("reflectoresConf")) reflectoresConf = doc["reflectoresConf"];
  if (doc.containsKey("luzPiletaConf")) luzPiletaConf = doc["luzPiletaConf"];
  if (doc.containsKey("luzGaleriaConf")) luzGaleriaConf = doc["luzGaleriaConf"];
  if (doc.containsKey("luzGaleriaBordeConf")) luzGaleriaBordeConf = doc["luzGaleriaBordeConf"];
  if (doc.containsKey("IntercambioST")) {
    bool nuevo = doc["IntercambioST"];
    if (nuevo != IntercambioST)
    {
        IntercambioST = nuevo;
        guardarIntercambioST();
    }
  }
  if (doc.containsKey("Telemetria")) Telemetria = doc["Telemetria"];

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

void reconnect()
{
  ArduinoOTA.handle();
  Serial.println("Intentando conectarse MQTT...");
  if (mqttClient.connect("esp32Client", mqttUser, mqttPassword))
  {
      Serial.println("Conectado");
      mqttClient.subscribe("BALH142N1788/Aveni793");
      bool buzzerState = false;
      for(int i = 0; i < 1000; i++){ // hace un poco de ruido
        buzzerState = !buzzerState;
        digitalWrite(SalidaBz, !buzzerState);
        delayMicroseconds(200);
      }
      delay(150);
      for(int i = 0; i < 1000; i++){ // hace un poco de ruido
        buzzerState = !buzzerState;
        digitalWrite(SalidaBz, !buzzerState);
        delayMicroseconds(100);
      }
      digitalWrite(SalidaBz, LOW);
  }
  else
  {
    Serial.print("Fallo, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" intentar de nuevo en 2 segundos");
    delay(2000);
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