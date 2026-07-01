#pragma once
#include <Arduino.h>
#include <globales.h>

// Inicialización
void sensoresInit();

// Lecturas generales
void leerSensores();

void guardarIntercambioST();

// Flags de estado
extern bool dhtOK;
extern bool dsOK_pileta;
extern bool dsOK_colector;
extern bool nivelOK;
extern bool luzOK;
extern float piletaRaw;
extern float colectorRaw;
extern bool IntercambioST;
