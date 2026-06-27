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

// Definición de pines
#define SenTyH 14
#define MovPIR2 13
#define MovPIR1 12
#define SalidaBz 16
#define Reflectores 1
#define Sirena 3
#define LuzVereda 5
#define Camaras 4
int fotoPin = A0;

// Definición de variables y constantes
const char *ssid = "pocha";
const char *wifiPassword = "Vale07Marce05";
const char *mqttServer = "broker.emqx.io";
const int mqttPort = 1883;
const char *mqttUser = "Marce";  // Reemplaza con tu usuario MQTT
const char *mqttPassword = "Aveni793";  // Reemplaza con tu contraseña MQTT
const char *EspUser = "Marce";  // Usuario para accederal ESP8266
const char *EspPassword = "EspAveni5";   // Contraseña para acceder al ESP8266

int CuentaMov = 0;
char Cuenta[46];
char Temp[20];
char Hum[20];
char Luz[25];
bool flagMov = false;
int var = 0;
char datos[40];
int TEMPERATURA = 0;
int HUMEDAD = 0;
int SirenaConf = 0;       //0 = Siempre apagada, 1 = Siempre encendida, 2 = Automático
bool CamarasEn = false;   //false = Encendidas, true = Apagadas (para resetearlas), Están conectadas al Normal Cerrado del relé
int ReflectoresConf = 0;  //0 = Siempre apagados, 1 = Siempre encendidos, 2 = Automático
int LuzVeredaConf = 0;    //0 = Siempre apagada, 1 = Siempre encendida, 2 = Automático
bool PIR1En = true;       //flag que habilita el sensor PIR1
bool PIR2En = true;       //flag que habilita el sensor PIR2
int SensorN = 0;          //Identifica que entrada de sensor se activó. 0 = Ninguno, 1 = PIR 1, 2 = PIR 2
volatile int secondCounter = 0;
volatile int enviaDataCounter = 0;

unsigned long previousMillis = 0;
const unsigned long interval = 10000;

Ticker secondTicker;
WiFiClient esp8266Client;
PubSubClient mqttClient(esp8266Client);
DHT dht(SenTyH, DHT11);

void callback(char *topic, byte *payload, unsigned int length) {
  char payload_string[length + 1];
  memcpy(payload_string, payload, length);
  payload_string[length] = '\0';

  int value = atoi(payload_string);   // Convertir el payload a un entero si es necesario

  // Procesar el mensaje basado en el tema
  if (strcmp(topic, "Iluminacion/Reflectores") == 0) {
    ReflectoresConf = value;  // Controlar Reflectores
  } else if (strcmp(topic, "Iluminacion/Vereda") == 0) {
    LuzVeredaConf = value;  // Controlar Luces de Vereda
  } else if (strcmp(topic, "Alarma/Sirena") == 0) {
    SirenaConf = value;  // Controlar Sirena
  } else if (strcmp(topic, "Alarma/Camaras") == 0) {
    CamarasEn = value;  // Controlar Camaras
  }
}

void enviarDatos() {
  StaticJsonDocument<500> doc;

  doc["Temperatura"] = TEMPERATURA;
  doc["Humedad"] = HUMEDAD;
  doc["Luz"] = Luz;
  doc["NumeroSensor"] = SensorN;
  doc["Movimientos"] = Cuenta;

  // Convertir el documento JSON en una cadena de texto
  char buffer[256];
  size_t n = serializeJson(doc, buffer);

  // Publicar el JSON en el topic
  mqttClient.publish("Ambiente/Datos", buffer, n);
}

void onSecondTick() {
  secondCounter++;
  enviaDataCounter++;
}

void reconnect() {
  while (!mqttClient.connected()) {
    //Serial.print("Intentando conectarse MQTT...");
    if (mqttClient.connect("esp8266Client", mqttUser, mqttPassword)) {
    //  Serial.println("Conectado");
      mqttClient.subscribe("Iluminacion/Reflectores");
      mqttClient.subscribe("Iluminacion/Vereda");
      mqttClient.subscribe("Alarma/Sirena");
      mqttClient.subscribe("Alarma/Camaras");
    } else {
    //  Serial.print("Fallo, rc=");
    //  Serial.print(mqttClient.state());
    //  Serial.println(" intentar de nuevo en 5 segundos");
      delay(5000);
    }
  }
}

void setup() {
  //Serial.begin(115200);
  //Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, wifiPassword);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    //Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.setHostname("Control_Frente");
  ArduinoOTA.setPassword("Aveni793");
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    //Serial.println("Start updating " + type);
    digitalWrite(Reflectores, HIGH);
    digitalWrite(Sirena, HIGH);
    digitalWrite(LuzVereda, HIGH);
    digitalWrite(Camaras, HIGH);
    digitalWrite(SalidaBz, HIGH);
    delay(250);
    digitalWrite(SalidaBz, LOW);
  });

  ArduinoOTA.onEnd([]() {
    //Serial.println("\nEnd");
    digitalWrite(Reflectores, HIGH);
    digitalWrite(Sirena, HIGH);
    digitalWrite(LuzVereda, HIGH);
    digitalWrite(Camaras, HIGH);
    CuentaMov = 0;
    digitalWrite(SalidaBz, HIGH);
    delay(250);
    digitalWrite(SalidaBz, LOW);
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    //Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      //Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      //Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      //Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      //Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      //Serial.println("End Failed");
    }
  });

  ArduinoOTA.begin();
  //Serial.println("Ready");
  //Serial.print("IP address: ");
  //Serial.println(WiFi.localIP());

  pinMode(MovPIR1, INPUT);
  pinMode(MovPIR2, INPUT);
  pinMode(SalidaBz, OUTPUT);
  pinMode(Reflectores, OUTPUT);
  pinMode(Sirena, OUTPUT);
  pinMode(LuzVereda, OUTPUT);
  pinMode(Camaras, OUTPUT);

  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(callback);

  digitalWrite(SalidaBz, LOW);
  digitalWrite(Reflectores, HIGH);
  digitalWrite(Sirena, HIGH);
  digitalWrite(LuzVereda, HIGH);
  digitalWrite(Camaras, HIGH);

  dht.begin();
  secondTicker.attach(1, onSecondTick);
  delay(2000);

}

void loop() {
  ArduinoOTA.handle();

  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  digitalWrite(SalidaBz, LOW);

  if (digitalRead(MovPIR1) && digitalRead(MovPIR2)) flagMov = false;

  if (!digitalRead(MovPIR1) && !flagMov) {
    flagMov = true;
    mqttClient.publish("Movimiento/PIR1", "Movimiento Detectado!!!");
    CuentaMov++;
    sprintf(Cuenta, "Cant. de movimientos detectados: %d ", CuentaMov);
    digitalWrite(SalidaBz, HIGH);
    delay(500);
    digitalWrite(SalidaBz, LOW);
  }

  if (!digitalRead(MovPIR2) && !flagMov) {
    flagMov = true;
    mqttClient.publish("Movimiento/PIR2", "Movimiento Detectado!!!");
    CuentaMov++;
    sprintf(Cuenta, "Cant. de movimientos detectados: %d ", CuentaMov);
    digitalWrite(SalidaBz, HIGH);
    delay(500);
    digitalWrite(SalidaBz, LOW);
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    TEMPERATURA = dht.readTemperature();
    if (isnan(TEMPERATURA)) {
      delay(100);
      TEMPERATURA = dht.readTemperature();
      if (isnan(TEMPERATURA)) {
        TEMPERATURA = -100;
      }
    }
    sprintf(Temp, "Temperatura: %d ", TEMPERATURA);
    mqttClient.publish("Ambiente/Temp", Temp);

    HUMEDAD = dht.readHumidity();
    if (isnan(HUMEDAD)) {
      delay(100);
      HUMEDAD = dht.readHumidity();
      if (isnan(HUMEDAD)){
        HUMEDAD = -1;
      }
    }
    sprintf(Hum, "Humedad: %d ", HUMEDAD);
    mqttClient.publish("Ambiente/Hum", Hum);

    int fotoval = analogRead(fotoPin);
    sprintf(Luz, "Es de %s: %d", (fotoval < 500) ? "día" : "noche", fotoval);
    mqttClient.publish("Ambiente/Luz", Luz);

    mqttClient.publish("Movimiento/PIRc", Cuenta);
  }

  if (ReflectoresConf == 0) {
    digitalWrite(Reflectores, HIGH);
  } else if (ReflectoresConf == 1) {
    digitalWrite(Reflectores, LOW);
  }

  if (SirenaConf == 0) {
    digitalWrite(Sirena, HIGH);
  } else if (SirenaConf == 1) {
    digitalWrite(Sirena, LOW);
  }

  if (LuzVeredaConf == 0) {
    digitalWrite(LuzVereda, HIGH);
  } else if (LuzVeredaConf == 1) {
    digitalWrite(LuzVereda, LOW);
  }

  digitalWrite(Camaras, CamarasEn == 1 ? LOW : HIGH);

  if (enviaDataCounter >= 9) {
    enviaDataCounter = 0;
    enviarDatos();
  }

}
