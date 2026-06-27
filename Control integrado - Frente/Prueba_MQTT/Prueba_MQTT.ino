#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <DHT_U.h>

// Definición de pines --------------

#define SenTyH 14
#define MovUW 13
#define MovPIR 12
#define SalidaBz 16
int ledPin = LED_BUILTIN;
int fotoPin = A0;

// Definición de variables y constantes ----------

const char *ssid = "pocha";
const char *password = "Vale07Marce05";
const char *mqttServer = "broker.emqx.io";
const int mqttPort = 1883;

int CuentaMov = 0;
char Cuenta[46];
char Temp[20];
char Hum[20];
char Luz[20];
bool flagMov = false;
int var = 0;
char datos[40];
// String resultS = "";
int TEMPERATURA = 0;
int HUMEDAD = 0;

unsigned long previousMillis = 0;     // Variable para almacenar el tiempo anterior
const unsigned long interval = 10000; // Intervalo de tiempo deseado en milisegundos

WiFiClient esp8266Client;
PubSubClient mqttClient(esp8266Client);

DHT dht(SenTyH, DHT11);

// Funciones ------

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("] ");

  char payload_string[length + 1];

  int resultI;

  memcpy(payload_string, payload, length);
  payload_string[length] = '\0';
  resultI = atoi(payload_string);

  var = resultI;
}

void reconnect()
{
  while (!mqttClient.connected())
  {
    Serial.print("Intentando conectarse MQTT...");

    if (mqttClient.connect("esp8266Client"))
    {
      Serial.println("Conectado");

      mqttClient.subscribe("LED_Azul/01");
    }
    else
    {
      Serial.print("Fallo, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" intentar de nuevo en 5 segundos");
      // Esperar 5 segundos antes de volver a intentar
      delay(5000);
    }
  }
}

// SETUP --------------------------------------------------
void setup()
{
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("Control_Frente");

  // No authentication by default
  ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) 
    {
      type = "sketch";
    } 
    else 
    {  // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type); 
    });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    digitalWrite(ledPin, HIGH);
    });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    digitalWrite(ledPin, LOW);
    });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) 
    {
      Serial.println("Auth Failed");
    }
    else if (error == OTA_BEGIN_ERROR)
    {
      Serial.println("Begin Failed");
    }
    else if (error == OTA_CONNECT_ERROR)
    {
      Serial.println("Connect Failed");
    }
    else if (error == OTA_RECEIVE_ERROR)
    {
      Serial.println("Receive Failed");
    }
    else if (error == OTA_END_ERROR)
    {
      Serial.println("End Failed");
    }
    });

  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  pinMode(ledPin, OUTPUT);
  pinMode(MovUW, INPUT);
  pinMode(MovPIR, INPUT);
  pinMode(SalidaBz, OUTPUT);
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(callback);
}

//Programa principal ---------------------------
void loop()
{
  ArduinoOTA.handle();

  if (!mqttClient.connected())
  {
    reconnect();
  }
  mqttClient.loop();

  digitalWrite(SalidaBz, LOW);

  if (var == 1)
  {
    digitalWrite(ledPin, LOW);
  }
  else if (var == 0)
  {
    digitalWrite(ledPin, HIGH);
  }

  if (!digitalRead(MovUW) && !flagMov)
  {
    flagMov = true;
    mqttClient.publish("Movimiento/uW", "Movmiento Detectado!!!");
    CuentaMov++;
    sprintf(Cuenta, "Cant. de movimientos detectados: %d ", CuentaMov);
    digitalWrite(SalidaBz, HIGH);
    delay(500);
    digitalWrite(SalidaBz, LOW);
  }
  if (digitalRead(MovUW))
    flagMov = false;

  unsigned long currentMillis = millis(); // Obtiene el tiempo actual
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    TEMPERATURA = dht.readTemperature();
    if (TEMPERATURA < -20 || TEMPERATURA > 99)
      TEMPERATURA = -100;
    sprintf(Temp, "Temperatura: %d ", TEMPERATURA);
    mqttClient.publish("Ambiente/Temp", Temp);
    HUMEDAD = dht.readHumidity();
    if (HUMEDAD < 0 || HUMEDAD > 99)
      HUMEDAD = -1;
    sprintf(Hum, "Humedad: %d ", HUMEDAD);
    mqttClient.publish("Ambiente/Hum", Hum);
    int fotoval = analogRead(fotoPin);
    if (fotoval < 500)
    {
      sprintf(Luz, "Es de día: %d ", fotoval);
    }
    else
    {
      sprintf(Luz, "Es de noche: %d ", fotoval);
    }
    
    mqttClient.publish("Ambiente/Luz", Luz);

    mqttClient.publish("Movimiento/uWc", Cuenta);
  }

  // delay(500);
}
