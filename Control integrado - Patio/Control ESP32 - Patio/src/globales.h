#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// === MAPEO DE PINES ESP32 ===
// Notas: elegir pines seguros para ESP32 (evitar GPIO1/GPIO3, y pines que comprometan el boot)
//SENSORES ===================================
#define SenTyH          32      // DHT22 data pin
#define pinDatoTemp     33      // Pin de datos de los sensores DS18B20
#define nivelAgua       13      // Sensor de nivel capacitivo
#define fotoPin         36      // ADC para LDR: usar GPIO36 (ADC1_CH0) — solo entrada analógica
#define Touch_MAS       39      //
#define Touch_MENOS     34      //
#define Touch_MENU       35      //

//COMUNICACION ================================
#define TXD_Nivel       1       //
#define RXD_Nivel       3       //
#define SCL_pin         22      //
#define SDA_pin         21      //


//ACTUADORES ==================================
#define SalidaBz        25      // Buzzer / Salida sonora
#define Reflectores     26      // Reflectores (relé)
#define bombaCisterna   27      // Salida que acciona la bomba de la Cisterna
#define bombaTanque     14      // Salida que acciona la bomba del Tanque del fondo
#define bombaPileta     12      // Salida que acciona la bomba de la Pileta
#define luzPileta       23      // Salida que acciona las luces sobre la Pileta
#define luzGaleria      19      // Salida que acciona las luces de la Galería
#define luzGaleriaBorde 18      // Salida que acciona las luces del borde de la Galería
#define salidaAux1      17      // Salida Auxiliar 1
#define salidaAux2      16      // Salida Auxiliar 2

// extern TFT_eSPI tft;
extern int var;
extern char datos[40];
extern float TEMPERATURA;       // Valor de Temperatura del sensor DHT22
extern float HUMEDAD;           // Valor de Temperatura del sensor DHT22
extern int Luz;                 // Lectura del cnversor ADC del sensor de luz
extern int ValorNoche;
extern int valNocheHist;
extern int histeresisLuz;
extern byte reflectoresConf;    // Las variables terminadas en "Conf" tendrán valores 0=Apagado, 1=Encendido, 2=Automático (estado definido por algoritmo)
extern bool reflectoresST;      // Las variables terminadas en "ST" serán flags de estado
extern byte bombaCisternaConf;
extern bool bombaCisternaST;
extern byte bombaTanqueConf;
extern bool bombaTanqueST;
extern byte bombaPiletaConf;
extern bool bombaPiletaST;
extern byte luzPiletaConf;
extern bool luzPiletaST;
extern byte luzGaleriaConf;
extern bool luzGaleriaST;
extern byte luzGaleriaBordeConf;
extern bool luzGaleriaBordeST;
extern bool aux1En;
extern bool aux2En;
extern float tempPileta;        // Temperatura del sensor DS18B20 en la pileta
extern float tempColector;      // Temperatura del sensor DS18B20 en el colector solar
extern float tempSet;           // Temperatura deseada en la Pileta
extern float tempDif_on;        // Set de temperatura diferencial entre pileta y colector para encendido de bomba
extern float tempDif_off;       // Set de temperatura diferencial entre pileta y colector para apagado de bomba
extern volatile int enviaDataCounter;   // Contador de tiempo en segundos para el envío de datos períodico
extern int intervalData;        // Intervalo de tiempo[seg] en el que se envían datos
extern bool Telemetria;         // Flag para pedir telemetría completa de todas las variables
extern bool nivelAguaST;        // true = agua presente
extern bool IntercambioST;


extern unsigned long ultimaAccionBomba;     // timestamp del último cambio
extern unsigned long tiempoEncendida;       // cuánto tiempo lleva encendida


// Configurables
extern unsigned long minOnTime;   // X min encendida mínimos
extern unsigned long minOffTime;   // X min apagada mínimo
extern unsigned long maxOnTime;   // X min máximo encendida

#define MQTT_MAX_PACKET_SIZE 1024

void crearEstadoJson(JsonDocument& doc);
void procesarComandosJson(JsonDocument& doc);