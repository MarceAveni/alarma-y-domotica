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
// OBJETOS Y HORARIOS
// =======================
#include <Preferences.h>

ScheduleRule schedules[8];
int ultimoMinutoEvaluado = -1;

bool obtenerHoraLocal(struct tm &infoTiempo) {
  time_t ahora = time(nullptr);
  if (ahora < 1600000000) {
    return false;
  }
  localtime_r(&ahora, &infoTiempo);
  return true;
}

void guardarSchedules() {
  Preferences prefs;
  prefs.begin("horarios", false);
  
  DynamicJsonDocument doc(1500);
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < 8; i++) {
    JsonObject obj = arr.createNestedObject();
    obj["active"] = schedules[i].active;
    obj["hour"] = schedules[i].hour;
    obj["minute"] = schedules[i].minute;
    obj["weekdays"] = schedules[i].weekdays;
    obj["target"] = schedules[i].target;
    obj["action"] = schedules[i].action;
  }
  
  String jsonStr;
  serializeJson(doc, jsonStr);
  prefs.putString("schedules", jsonStr);
  prefs.end();
  Serial.println("Horarios guardados en Preferences.");
}

void cargarSchedules() {
  Preferences prefs;
  prefs.begin("horarios", true);
  String jsonStr = prefs.getString("schedules", "");
  prefs.end();

  // Inicializar por defecto
  for (int i = 0; i < 8; i++) {
    schedules[i].active = false;
    schedules[i].hour = 0;
    schedules[i].minute = 0;
    schedules[i].weekdays = 0;
    schedules[i].target = 0;
    schedules[i].action = 0;
  }

  if (jsonStr.length() > 0) {
    DynamicJsonDocument doc(1500);
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (!error) {
      JsonArray arr = doc.as<JsonArray>();
      int idx = 0;
      for (JsonObject obj : arr) {
        if (idx >= 8) break;
        schedules[idx].active = obj["active"] | false;
        schedules[idx].hour = obj["hour"] | 0;
        schedules[idx].minute = obj["minute"] | 0;
        schedules[idx].weekdays = obj["weekdays"] | 0;
        schedules[idx].target = obj["target"] | 0;
        schedules[idx].action = obj["action"] | 0;
        idx++;
      }
      Serial.println("Horarios cargados exitosamente de Preferences.");
    } else {
      Serial.println("Fallo al deserializar horarios de Preferences.");
    }
  } else {
    Serial.println("No hay horarios guardados en Preferences.");
  }
}

Ticker secondTicker;

void crearEstadoJson(JsonDocument& doc)
{
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
  doc["aux1En"] = aux1En;
  doc["aux2En"] = aux2En;

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
}

void procesarComandosJson(JsonDocument& doc)
{
  bool configChanged = false;

  if (doc.containsKey("intervalData")) { intervalData = doc["intervalData"]; }
  if (doc.containsKey("tempDif_on")) { tempDif_on = doc["tempDif_on"]; }
  if (doc.containsKey("tempDif_off")) { tempDif_off = doc["tempDif_off"]; }
  if (doc.containsKey("tempSet")) { tempSet = doc["tempSet"]; }
  if (doc.containsKey("minOnTime")) { minOnTime = doc["minOnTime"]; }
  if (doc.containsKey("minOffTime")) { minOffTime = doc["minOffTime"]; }
  if (doc.containsKey("maxOnTime")) { maxOnTime = doc["maxOnTime"]; }
  
  if (doc.containsKey("bombaPiletaConf")) { bombaPiletaConf = doc["bombaPiletaConf"]; }
  if (doc.containsKey("bombaCisternaConf")) { bombaCisternaConf = doc["bombaCisternaConf"]; }
  if (doc.containsKey("bombaTanqueConf")) { bombaTanqueConf = doc["bombaTanqueConf"]; }
  if (doc.containsKey("reflectoresConf")) { reflectoresConf = doc["reflectoresConf"]; }
  if (doc.containsKey("luzPiletaConf")) { luzPiletaConf = doc["luzPiletaConf"]; }
  if (doc.containsKey("luzGaleriaConf")) { luzGaleriaConf = doc["luzGaleriaConf"]; }
  if (doc.containsKey("luzGaleriaBordeConf")) { luzGaleriaBordeConf = doc["luzGaleriaBordeConf"]; }
  
  if (doc.containsKey("aux1En")) { aux1En = doc["aux1En"]; }
  if (doc.containsKey("aux2En")) { aux2En = doc["aux2En"]; }
  
  if (doc.containsKey("IntercambioST")) {
    bool nuevo = doc["IntercambioST"];
    if (nuevo != IntercambioST)
    {
        IntercambioST = nuevo;
        guardarIntercambioST();
    }
  }
  
  if (doc.containsKey("patioSchedules") || doc.containsKey("schedules")) {
    JsonArray schedArr = doc.containsKey("patioSchedules") ? doc["patioSchedules"].as<JsonArray>() : doc["schedules"].as<JsonArray>();
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
    guardarSchedules();
    configChanged = true;
  }
  
  if (doc.containsKey("Telemetria")) Telemetria = doc["Telemetria"];
  
  if (configChanged) {
    actuadores();
  }
}

void enviarDatos()
{
  DynamicJsonDocument doc(2048);
  crearEstadoJson(doc);

  char buffer[1500];
  size_t n = serializeJson(doc, buffer, sizeof(buffer));

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

      // 0: Reflectores, 1: Luz Pileta, 2: Luz Galería, 3: Luz Galería Borde, 4: Bomba Pileta, 5: Bomba Cisterna, 6: Bomba Tanque
      if (target == 0) {
        reflectoresConf = action;
        configChanged = true;
        Serial.printf("Horario: reflectoresConf cambiado a %d\n", action);
      } else if (target == 1) {
        luzPiletaConf = action;
        configChanged = true;
        Serial.printf("Horario: luzPiletaConf cambiado a %d\n", action);
      } else if (target == 2) {
        luzGaleriaConf = action;
        configChanged = true;
        Serial.printf("Horario: luzGaleriaConf cambiado a %d\n", action);
      } else if (target == 3) {
        luzGaleriaBordeConf = action;
        configChanged = true;
        Serial.printf("Horario: luzGaleriaBordeConf cambiado a %d\n", action);
      } else if (target == 4) {
        bombaPiletaConf = action;
        configChanged = true;
        Serial.printf("Horario: bombaPiletaConf cambiado a %d\n", action);
      } else if (target == 5) {
        bombaCisternaConf = action;
        configChanged = true;
        Serial.printf("Horario: bombaCisternaConf cambiado a %d\n", action);
      } else if (target == 6) {
        bombaTanqueConf = action;
        configChanged = true;
        Serial.printf("Horario: bombaTanqueConf cambiado a %d\n", action);
      }
    }
  }

  if (configChanged) {
    actuadores();
    enviarDatos();
  }
}

// =======================
// SETUP
// =======================
void setup()
{
  Serial.begin(115200);

  wifiConect();
  
  // Inicializar NTP para Argentina (GMT-3)
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  
  // Cargar horarios desde Preferences
  cargarSchedules();

  otaConfig();
  mqttConfig();
  serverConfig();

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
  handleMqtt();
  server.handleClient();
  
  evaluarHorarios();
  
  leerSensores();
  controlBombaPileta();
  actuadores();

  if (Telemetria)
  {
    enviarDatos();
    Telemetria = false;
  }
}

