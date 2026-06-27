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
//#define MQTT_MAX_PACKET_SIZE 1024


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

// Definición de variables y constantes --------------------------------------------------------------
int intervalData = 60;                // Intervalo de tiempo[seg] en el que se envían datos
bool Telemetria = false;               // Flag para pedir telemetría completa de todas las variables
const char *ssid = "pocha";
const char *wifiPassword = "Vale07Marce05";
const char *mqttServer = "broker.emqx.io";
const int mqttPort = 1883;
const char *mqttUser = "Marce";        // Reemplaza con tu usuario MQTT
const char *mqttPassword = "Aveni793"; // Reemplaza con tu contraseña MQTT
const char *EspUser = "Marce";         // Usuario para accederal ESP8266
const char *EspPassword = "EspAveni5"; // Contraseña para acceder al ESP8266

int CuentaMov = 0;
int movTime = 6;              // Tiempo [seg] de espera entre la primer detección de movimiento y la segunda
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
int sirenaTime = 5;           // Tiempo [seg] de encendido de la sirena cuando se detecta movimiento
int movRst = 60;              // Tiempo [seg] de reseteo de mvimientos para activación de sirena
bool CamarasEn = true;        // true = Encendidas, false = Apagadas (para resetearlas), Están conectadas al Normal Cerrado del relé
int ReflectoresConf = 2;      // 0 = Siempre apagados, 1 = Siempre encendidos, 2 = Automático
bool reflectoresST = false;   // Flag de estado de los reflectores
int refTime = 300;            // Tiempo [seg] de encendido de los reflectores cuando se detecta movimiento
int LuzVeredaConf = 0;        // 0 = Siempre apagada, 1 = Siempre encendida, 2 = Automático
bool luzVeredaST = false;     // Flag de estado de las luces de Vereda
bool PIR1En = true;           // flag que habilita el sensor PIR1
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


void callback(char *topic, byte *payload, unsigned int length)
{
  // Crea un buffer para almacenar el mensaje JSON
  char payload_string[length + 1];
  memcpy(payload_string, payload, length);
  payload_string[length] = '\0'; // Terminar la cadena con NULL

  // Crea un objeto JSON usando ArduinoJson
  StaticJsonDocument<800> doc;

  // Intenta parsear el payload como JSON
  DeserializationError error = deserializeJson(doc, payload_string);
  
  // Verifica si hubo errores en el parsing
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

  // Extrae los valores del JSON y asigna a las variables correspondientes
  if (doc.containsKey("ReflectoresConf"))   ReflectoresConf = doc["ReflectoresConf"];
  if (doc.containsKey("LuzVeredaConf"))     LuzVeredaConf = doc["LuzVeredaConf"];
  if (doc.containsKey("SirenaConf"))        SirenaConf = doc["SirenaConf"];
  if (doc.containsKey("CamarasEn"))         CamarasEn = doc["CamarasEn"];
  if (doc.containsKey("intervalData"))      intervalData = doc["intervalData"];
  if (doc.containsKey("movTime"))           movTime = doc["movTime"];
  if (doc.containsKey("ValorNoche"))        ValorNoche = doc["ValorNoche"];
  if (doc.containsKey("HisteresisLuz"))     histeresisLuz = doc["HisteresisLuz"];
  if (doc.containsKey("sirenaTime"))        sirenaTime = doc["sirenaTime"];
  if (doc.containsKey("refTime"))           refTime = doc["refTime"];
  if (doc.containsKey("movRst"))            movRst = doc["movRst"];
  if (doc.containsKey("PIR1En"))            PIR1En = doc["PIR1En"];
  if (doc.containsKey("PIR2En"))            PIR2En = doc["PIR2En"];
  if (doc.containsKey("Telemetria"))        Telemetria = doc["Telemetria"];
}

void enviarDatos()
{
  StaticJsonDocument<500> doc;

  doc["Temperatura"] = TEMPERATURA;
  doc["Humedad"] = HUMEDAD;
  doc["Luz"] = Luz;
  doc["Sensor"] = SensorN;
  doc["Movimientos"] = CuentaMov;
  doc["Camaras"] = CamarasEn;
  doc["Sirena"] = sirenaST;
  doc["Reflectores"] = reflectoresST;
  doc["Luz Vereda"] = luzVeredaST;

  // Convertir el documento JSON en una cadena de texto
  char buffer[256];
  size_t n = serializeJson(doc, buffer, sizeof(buffer));

  // Publicar el JSON en el topic
  mqttClient.publish("BALH142N1788/Frente", buffer, n);     // BA = Buenos Aires, LH = Los Hornos, Calle 142 N1788 - Frente
}

void enviarTelemetria()
{
  StaticJsonDocument<500> doc;

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

  char buffer[256];

  size_t n = serializeJson(doc, buffer, sizeof(buffer));

  mqttClient.publish("BALH142N1788/Frente", buffer, n);
  Telemetria = false;
}

void onSecondTick()
{
  enviaDataCounter++;
  if (movCount  > 0)
  movCount--;
  if (refCount  > 0)
  refCount--;
  if (sirenaCount  > 0)
  sirenaCount--;
  if (movRstCount > 0)
  movRstCount--;
}

void alarma()
{
  refCount = refTime;
  movRstCount = movRst;
  if (CuentaMov <= 1) movCount = movTime;   // Cargo el contador de tiempo entre el primer movimiento y el segundo
  if (CuentaMov >= 2 && !sirenaST) sirenaCount = sirenaTime;
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
  if (SirenaConf == 1 || (SirenaConf == 2 && sirenaCount > 0 ))
  {
    digitalWrite(Sirena, LOW);        // Enciende sirena
    sirenaST = true;
  }
  else 
  {
    digitalWrite(Sirena, HIGH);    // Cualquier otro caso, la apaga
    sirenaST = false;
  }

  if (ReflectoresConf == 1 || (ReflectoresConf == 2 && refCount > 0 && fotoval > valNocheHist))
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

  digitalWrite(Camaras, CamarasEn ? HIGH : LOW); // Para las cámaras se usa el NC del relé, pero el módulo de relé es de lógica inversa
}

void reconnect()
{
  while (!mqttClient.connected())
  {
    ArduinoOTA.handle();
    // Serial.print("Intentando conectarse MQTT...");
    if (mqttClient.connect("esp8266Client", mqttUser, mqttPassword))
    {
      //  Serial.println("Conectado");
      mqttClient.subscribe("BALH142N1788/Aveni793");
    }
    else
    {
      //  Serial.print("Fallo, rc=");
      //  Serial.print(mqttClient.state());
      //  Serial.println(" intentar de nuevo en 5 segundos");
      delay(5000);
    }
  }
}

void setup()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, wifiPassword);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.setHostname("Control_Frente");
  ArduinoOTA.setPassword("Aveni793");

  ArduinoOTA.onStart([]() 
  {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
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
    //Serial.println("\nEnd");
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

  ArduinoOTA.onError([](ota_error_t error)
  {
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

void loop()
{
  ArduinoOTA.handle();

  if (!mqttClient.connected()) reconnect();

  mqttClient.loop();

  digitalWrite(SalidaBz, LOW);

  if ((!digitalRead(MovPIR1) && !digitalRead(MovPIR2)) || (movCount == 0 && sirenaCount == 0)) flagMov = false; // Me permite resetaer después de los movTime segundos del primer movimiento siempre que no esté sonando la sirena

  if (PIR1En && digitalRead(MovPIR1) && !flagMov)
  {
    flagMov = true;
    CuentaMov++;
    SensorN = 1;
    enviarDatos();
    digitalWrite(SalidaBz, HIGH);
    delay(250);
    digitalWrite(SalidaBz, LOW);
    alarma();
    SensorN = 0;
  }

  if (PIR2En && digitalRead(MovPIR2) && !flagMov)
  {
    flagMov = true;
    CuentaMov++;
    SensorN = 2;
    enviarDatos();
    digitalWrite(SalidaBz, HIGH);
    delay(250);
    digitalWrite(SalidaBz, LOW);
    alarma();
    SensorN = 0;
  }

  if (enviaDataCounter >= intervalData)
  {
    enviaDataCounter = 0;
    mediciones();
    enviarDatos();
  }

  valNocheHist = reflectoresST ? (ValorNoche - histeresisLuz) : (ValorNoche + histeresisLuz);

  actuadores();

  if (movRstCount == 0) CuentaMov = 0;     // La cuenta de cantidad de movimientos se resetea luego de movRST segundos

  if (Telemetria) 
  {
    mediciones();
    enviarDatos();
    delay(500);
    enviarTelemetria();
  }

}
