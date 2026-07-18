#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("--- Firmware Patio ESP32-S3 Iniciado ---");
}

void loop() {
    // Código principal para el Patio (Temperatura, Nivel de agua, Relés de filtrado/colectores)
}
