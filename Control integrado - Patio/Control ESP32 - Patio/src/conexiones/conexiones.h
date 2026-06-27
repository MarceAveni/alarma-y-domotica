#pragma once
#include <Arduino.h>
#include <globales.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>

extern WiFiClient espClient;
extern PubSubClient mqttClient;
extern WebServer server;

enum WiFiEstado {
    WIFI_CONECTANDO,
    WIFI_CONECTADO,
    WIFI_DESCONECTADO
};

extern WiFiEstado wifiEstado;

extern uint8_t wifiIntentos;
extern unsigned long ultimoIntentoWiFi;
extern unsigned long ultimoOKWiFi;

extern const uint8_t MAX_INTENTOS_WIFI;
extern const unsigned long INTERVALO_INTENTO_WIFI;
extern const unsigned long TIEMPO_MAX_SIN_WIFI;

void callback(char *topic, byte *payload, unsigned int length);
void wifiConect();
void wifiLoop();
void otaConfig();
void mqttConfig();
void reconnect();