// =========================
// Firmware ESP32 – Etapa 1
// =========================

#include <Arduino.h>
#include <globales.h>
#include <conexiones/conexiones.h>
#include <sensores/sensores.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>

// =======================
// VARIABLES GLOBALES
// =======================
float TEMPERATURA = 0;
float HUMEDAD = 0;
int Luz = 0;

float tempPileta = 0;
float tempColector = 0;

int ValorNoche = 500;
int valNocheHist = 0;
int histeresisLuz = 20;

byte reflectoresConf = 0;
bool reflectoresST = false;

byte bombaCisternaConf = 0;
bool bombaCisternaST = false;

byte bombaTanqueConf = 0;
bool bombaTanqueST = false;

byte bombaPiletaConf = 0;
bool bombaPiletaST = false;

byte luzPiletaConf = 0;
bool luzPiletaST = false;

byte luzGaleriaConf = 0;
bool luzGaleriaST = false;

byte luzGaleriaBordeConf = 0;
bool luzGaleriaBordeST = false;

bool aux1En = false;
bool aux2En = false;

float tempSet = 35.0;
float tempDif_on = 10.0;  // Diferencia para encender bomba
float tempDif_off = 5.0;  // Diferencia para apagar bomba

volatile int enviaDataCounter = 0;
int intervalData = 60;
bool Telemetria = false;

int var = 0;
char datos[40];

unsigned long ultimaAccionBomba = 0;     // timestamp del último cambio
unsigned long tiempoEncendida = 0;       // cuánto tiempo lleva encendida

bool IntercambioST = false;
float piletaRaw = 0;
float colectorRaw = 0;

// Configurables
unsigned long minOnTime  = 60000;   // 1 min encendida mínimos
unsigned long minOffTime =  300000;   // 5 min apagada mínimo
unsigned long maxOnTime  = 900000;   // 15 min máximo encendida

// =======================
// OBJETOS
// =======================
Ticker secondTicker;

// =======================
// PUBLI TELEMETRÍA – RECICLADO
// =======================
void enviarDatos()
{
  StaticJsonDocument<1000> doc;

  doc["tempPileta"] = tempPileta;
  doc["tempColector"] = tempColector;
  doc["bombaPiletaST"] = bombaPiletaST;
  doc["bombaPiletaConf"] = bombaPiletaConf;
  doc["tempSet"] = tempSet;
  doc["tempDif_on"] = tempDif_on;
  doc["tempDif_off"] = tempDif_off;
  doc["minOnTime"] = minOnTime;
  doc["minOffTime"] = minOffTime;
  doc["maxOnTime"] = maxOnTime;
  doc["bombaCisternaST"] = bombaCisternaST;
  doc["bombaCisternaConf"] = bombaCisternaConf;
  doc["bombaTanqueST"] = bombaTanqueST;
  doc["bombaTanqueConf"] = bombaTanqueConf;
  doc["reflectoresST"] = reflectoresST;
  doc["reflectoresConf"] = reflectoresConf;
  doc["luzPiletaST"] = luzPiletaST;
  doc["luzPiletaConf"] = luzPiletaConf;
  doc["luzGaleriaST"] = luzGaleriaST;
  doc["luzGaleriaConf"] = luzGaleriaConf;
  doc["luzGaleriaBordeST"] = luzGaleriaBordeST;
  doc["luzGaleriaBordeConf"] = luzGaleriaBordeConf;
  doc["luzAmbiente"] = Luz;
  doc["nivelAgua"] = nivelAguaST;
  doc["tempTecho"] = TEMPERATURA;
  doc["humTecho"] = HUMEDAD;
  doc["IntercambioST"] = IntercambioST;
  doc["tempPiletaCrudo"] = piletaRaw;
  doc["tempColectorCrudo"] = colectorRaw;

  char buffer[1024];
  size_t n = serializeJson(doc, buffer);

  mqttClient.publish("BALH142N1788/Patio", buffer, n);
}

// =======================
// BOMBA POR DIFERENCIA TÉRMICA
// =======================
void controlBombaPileta()
{
  float dif = tempColector - tempPileta;
  unsigned long ahora = millis();

  bool estadoPrevio = bombaPiletaST;   // Guardar estado anterior

  switch (bombaPiletaConf)
  {
      // =====================================================
      // 1 - Encendido manual
      // =====================================================
      case 1:
          if (!bombaPiletaST) {
              digitalWrite(bombaPileta, HIGH);   // ENCENDER (lógica directa)
              bombaPiletaST = true;
              ultimaAccionBomba = ahora;
              tiempoEncendida = ahora;
          }
          break;

      // =====================================================
      // 2 - Modo automático
      // =====================================================
      case 2:
      {
          bool deseoEncender =  (dif >= tempDif_on) && (tempPileta < tempSet);
          bool deseoApagar   =  (dif <= tempDif_off) || (tempPileta >= tempSet);

          // -------------------------------
          // MAX ON TIME (protección)
          // -------------------------------
          if (bombaPiletaST && (ahora - tiempoEncendida >= maxOnTime))
          {
              digitalWrite(bombaPileta, LOW);    // APAGAR
              bombaPiletaST = false;
              ultimaAccionBomba = ahora;
              break;
          }

          // -------------------------------
          // MIN OFF TIME (protección)
          // -------------------------------
          if (!bombaPiletaST && deseoEncender)
          {
              if (ahora - ultimaAccionBomba < minOffTime)
                  break;
          }

          // -------------------------------
          // MIN ON TIME (protección)
          // -------------------------------
          if (bombaPiletaST && deseoApagar)
          {
              if (ahora - tiempoEncendida < minOnTime)
                  break;
          }

          // -------------------------------
          // ENCENDER por condición térmica
          // -------------------------------
          if (deseoEncender && !bombaPiletaST)
          {
              digitalWrite(bombaPileta, HIGH);   // ENCENDER
              bombaPiletaST = true;
              ultimaAccionBomba = ahora;
              tiempoEncendida = ahora;
          }
          // -------------------------------
          // APAGAR por condición térmica
          // -------------------------------
          else if (deseoApagar && bombaPiletaST)
          {
              digitalWrite(bombaPileta, LOW);    // APAGAR
              bombaPiletaST = false;
              ultimaAccionBomba = ahora;
          }
      }
      break;

      // =====================================================
      // 0 y otros – apagado seguro
      // =====================================================
      default:
          if (bombaPiletaST) {
              digitalWrite(bombaPileta, LOW);     // APAGAR
              bombaPiletaST = false;
              ultimaAccionBomba = ahora;
          }
          break;
  }

  // =====================================================
  // Publicación automática solo si hubo cambio real
  // =====================================================
  if (bombaPiletaST != estadoPrevio)
      enviarDatos();
}

// =======================
// TICKER
// =======================
void cadaSegundo()
{
  enviaDataCounter++;
  if (enviaDataCounter >= intervalData)
  {
    enviaDataCounter = 0;
    Telemetria = true;
  }
}

void actuadores()
{
  if (bombaPiletaConf == 0)
  {
    digitalWrite(bombaPileta, LOW);       // Apaga la bomba
    bombaPiletaST = false;
  }
  else if (bombaPiletaConf == 1)
  {
    digitalWrite(bombaPileta, HIGH);    // Prende la bomba
    bombaPiletaST = true;
  }

  if (bombaCisternaConf == 0)
  {
    digitalWrite(bombaCisterna, HIGH);       // Apaga la bomba
    bombaCisternaST = false;
  }
  else if (bombaCisternaConf == 1)
  {
    digitalWrite(bombaCisterna, LOW);    // Prende la bomba
    bombaCisternaST = true;
  }

  if (bombaTanqueConf == 0)
  {
    digitalWrite(bombaTanque, HIGH);       // Apaga la bomba
    bombaTanqueST = false;
  }
  else if (bombaTanqueConf == 1)
  {
    digitalWrite(bombaTanque, LOW);    // Prende la bomba
    bombaTanqueST = true;
  }

  if (reflectoresConf == 1 || (reflectoresConf == 2 && Luz > valNocheHist))
  {
    digitalWrite(Reflectores, HIGH);       // Enciende reflectores
    reflectoresST = true;
  }
  else 
  {
    digitalWrite(Reflectores, LOW);    // Cualquier otro caso, la apaga
    reflectoresST = false;
  }

  if (luzPiletaConf == 1 || (luzPiletaConf == 2 && Luz > valNocheHist))
  {
    digitalWrite(luzPileta, LOW);       // Enciende luz de galeria
    luzPiletaST = true;
  }
  else 
  {
    digitalWrite(luzPileta, HIGH);    // Cualquier otro caso, la apaga
    luzPiletaST = false;
  }

  if (luzGaleriaConf == 1 || (luzGaleriaConf == 2 && Luz > valNocheHist))
  {
    digitalWrite(luzGaleria, LOW);       // Enciende luz de galeria
    luzGaleriaST = true;
  }
  else 
  {
    digitalWrite(luzGaleria, HIGH);    // Cualquier otro caso, la apaga
    luzGaleriaST = false;
  }

  if (luzGaleriaBordeConf == 1 || (luzGaleriaBordeConf == 2 && Luz > valNocheHist))
  {
    digitalWrite(luzGaleriaBorde, LOW);       // Enciende luz de galeria
    luzGaleriaBordeST = true;
  }
  else 
  {
    digitalWrite(luzGaleriaBorde, HIGH);    // Cualquier otro caso, la apaga
    luzGaleriaBordeST = false;
  }

  digitalWrite(salidaAux1, aux1En ? LOW : HIGH);  // Relé de lógica inversa
  digitalWrite(salidaAux2, aux2En ? LOW : HIGH);  // Relé de lógica inversa
}

// =======================
// SETUP
// =======================
void setup()
{
  Serial.begin(115200);

  wifiConect();
  otaConfig();
  mqttConfig();

  pinMode(SalidaBz, OUTPUT);
  digitalWrite(SalidaBz, LOW);
  pinMode(bombaPileta, OUTPUT);
  digitalWrite(bombaPileta, LOW);
  pinMode(bombaCisterna, OUTPUT);
  digitalWrite(bombaCisterna, HIGH);
  pinMode(bombaTanque, OUTPUT);
  digitalWrite(bombaTanque, HIGH);
  pinMode(Reflectores, OUTPUT);
  digitalWrite(Reflectores, LOW);
  pinMode(luzPileta, OUTPUT);
  digitalWrite(luzPileta, HIGH);
  pinMode(luzGaleria, OUTPUT);
  digitalWrite(luzGaleria, HIGH);
  pinMode(luzGaleriaBorde, OUTPUT);
  digitalWrite(luzGaleriaBorde, HIGH);
  pinMode(salidaAux1, OUTPUT);
  digitalWrite(salidaAux1, HIGH);
  pinMode(salidaAux2, OUTPUT);
  digitalWrite(salidaAux2, HIGH);

  pinMode(nivelAgua, INPUT_PULLUP);
  pinMode(Touch_MAS, INPUT_PULLUP);
  pinMode(Touch_MENOS, INPUT_PULLUP);
  pinMode(Touch_MENU, INPUT_PULLUP);

  sensoresInit();

  secondTicker.attach(1, cadaSegundo);

  delay(2000);

}

// =======================
// LOOP
// =======================
void loop()
{
  wifiLoop();
  ArduinoOTA.handle();

  mqttClient.loop();
  delay(250);
  if (!mqttClient.connected())
  {
    reconnect();
  }
  
  leerSensores();
  controlBombaPileta();
  actuadores();

  if (Telemetria)
  {
    enviarDatos();
    Telemetria = false;
  }

  leerDS18_crudo();

}

