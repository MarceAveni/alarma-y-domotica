# Alarma y Domótica del Hogar

Este repositorio unifica el código fuente de los firmwares, modelos 3D, esquemas de hardware y documentación general de los sistemas de automatización y seguridad instalados en el hogar.

## 🚀 Versión 1.0 - Versión Inicial Estable
Esta es la primera versión completada, probada y en funcionamiento del sistema integrado.

---

## 📂 Estructura del Repositorio

*   **`Control integrado - Frente/`**: Firmware para el módulo ESP8266 que controla la seguridad del frente, sensores de movimiento PIR, sirena, reflectores y luces de vereda.
*   **`Control integrado - Patio/`**: Firmware para el módulo ESP32 que controla la temperatura de la piscina (pileta), colectores térmicos solares, nivel de agua y luces de la galería/patio.
*   **`Diseño y Hardware/`**:
    *   `Modelos 3D y Planos/`: Planos CAD (.dwg) y archivos STL listos para imprimir en 3D del gabinete central, gabinete frente y soportes generales.
    *   `Desarrollo de Hardware/`: Documentación técnica (pinouts) y directorios reservados para futuras revisiones de placas.

---

## 📡 Arquitectura de Comunicaciones (MQTT)

Ambas placas están conectadas a la red WiFi local y se comunican a través de un broker MQTT público (`broker.emqx.io`).

*   **Topic de Comandos (Escuchan ambos):** `BALH142N1788/Aveni793`
    *   Ambos dispositivos procesan mensajes JSON en este canal y extraen solo las variables de control que les corresponden.
*   **Topic de Telemetría Frente (ESP8266):** `BALH142N1788/Frente`
*   **Topic de Telemetría Patio (ESP32):** `BALH142N1788/Patio`

---

## 🛠️ Herramientas de Desarrollo

*   **Firmware:** [PlatformIO](https://platformio.org/) integrado en VS Code.
*   **Carga de Firmware:** Soporta programación inalámbrica **OTA** (Over The Air) localmente en ambos módulos.
*   **Espacio de Trabajo de VS Code:** Abrir el archivo `Control ESP32 - Patio.code-workspace` en la raíz para cargar ambos proyectos de firmware simultáneamente.
