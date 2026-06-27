#include <ESP8266WiFi.h>
#include <PubSubClient.h>

WiFiClient esp8266Client;
PubSubClient mqttClient(esp8266Client);

const char* ssid = "pocha";
const char* password = "Vale07Marce05";
const char* mqttServer = "broker.emqx.io";
const int mqttPort = 1883;

int ledPin = LED_BUILTIN; // El LED incorporado en el ESP8266 puede variar según el modelo
int fotoPin = A0; // Cambiado a A0 para leer un pin analógico

int var = 0;
char datos[40];
String resultS = "";

void wifiInit() {
  Serial.begin(115200);
  delay(10);
  Serial.print("Conectándose a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("Conectado a WiFi");
  Serial.println("Dirección IP: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("] ");

  char payload_string[length + 1];

  int resultI;

  memcpy(payload_string, payload, length);
  payload_string[length] = '\0';
  resultI = atoi(payload_string);

  var = resultI;

  resultS = "";

  for (int i = 0; i < length; i++) {
    resultS = resultS + (char)payload[i];
  }
  Serial.println();
}

void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Intentando conectarse MQTT...");

    if (mqttClient.connect("esp8266Client")) {
      Serial.println("Conectado");

      mqttClient.subscribe("LED_Azul/01");
    } else {
      Serial.print("Fallo, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" intentar de nuevo en 5 segundos");
      // Esperar 5 segundos antes de volver a intentar
      delay(5000);
    }
  }
}

void setup() {
  pinMode(ledPin, OUTPUT);
  wifiInit();
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(callback);
}

void loop() {
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  Serial.print("String: ");
  Serial.println(resultS);

  if (var == 0) {
    digitalWrite(ledPin, LOW);
  } else if (var == 1) {
    digitalWrite(ledPin, HIGH);
  }

  int fotoval = analogRead(fotoPin);
  Serial.print("Foto: ");
  Serial.println(fotoval);

  sprintf(datos, "Valor ADC: %d ", fotoval);
  mqttClient.publish("ADC_Wifi/01", datos);
  delay(5000);
}
