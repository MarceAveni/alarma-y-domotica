# Documento de Requerimientos - Placa Universal ESP32-S3

Este documento define las especificaciones técnicas, eléctricas y funcionales para el diseño del hardware de la Placa Universal basada en el **ESP32-S3**. Es un documento vivo que se irá puliendo a medida que definamos más detalles del circuito.

---

## 1. Resumen de Requerimientos de Entrada/Salida y Protocolos

El objetivo es tener un hardware único y robusto que pueda cubrir todas las necesidades de la casa (alarma, piscina, luces, sensores):

*   **16 Entradas Digitales**: Aisladas mediante optoacopladores para cables largos (sensores PIR, magnéticos, pulsadores).
*   **8 Salidas de Relé de Estado Sólido (SSR) en placa**: Tipo **G3MB-202P** (salida 100-240VAC, máx 2A, cruce por cero), ideal para activar luces o cargas pequeñas de alterna sin desgaste mecánico ni ruido electromagnético.
*   **4 Entradas Analógicas**: Para medición de nivel de agua (sensor de presión/corriente), monitoreo de voltaje de batería, etc.
*   **4 Salidas para SSR de Potencia Externos**: Señales de control de 5V/12V DC para accionar relés de estado sólido industriales de alta potencia externos (ej. 60A / 24-380VAC para bombas de piscina, calentadores, reflectores de alta potencia).
*   **4 Salidas MOSFET de Potencia en placa**: Para cargas DC de 12V (Sirena de alarma, tiras LED, electroválvulas de riego, etc.).
*   **1 Puerto para Sensor de Nivel Ultrasonido**: Compatible con sensores impermeables **JSN-SR04T** / **AJ-SR04M** (requiere 5V y pines de Trigger/Echo o UART).
*   **Buses de Comunicación**:
    *   **I2C**: Para displays (OLED, LCD), expansores y sensores de clima (BME280, AHT20).
    *   **SPI**: Para lector de tarjetas (RFID) o pantallas TFT rápidas.
    *   **1-Wire**: Para sensores de temperatura DS18B20 (múltiples sensores direccionados en un solo cable).
    *   **RS485 (Opcional)**: Aislado galvánicamente para protocolos Modbus industriales.
    *   **Ethernet (Opcional)**: Mediante chip SPI externo (ej. **W5500**) para redundancia cableada si falla el WiFi.

---

## 2. Estudio de Pines del ESP32-S3 (DevKitC-1 de 38 / 44 pines)

El módulo **ESP32-S3-WROOM-1** expone alrededor de **30 a 32 GPIOs útiles**. Muchos pines tienen funciones compartidas o restricciones de arranque (*strapping pins*), como GPIO 0 y GPIO 46.

Hacer un diseño con todas estas entradas y salidas directas al microcontrolador supera la cantidad de pines nativos disponibles. Por ello, analizamos dos opciones de diseño:

---

### OPCIÓN A: Sin Expansores de Puertos (Pines Nativos)

Para diseñar la placa sin chips integrados adicionales, debemos optimizar y reducir la cantidad de entradas y salidas al máximo para no superar los ~26 GPIOs libres reales.

#### Asignación de Pines Reducida (Ejemplo Óptimo):
1.  **Entradas Digitales Optoacopladas**: Reducido a **6 entradas** (ej. 3 zonas de alarma + 3 pulsadores).
2.  **Salidas SSR G3MB-202P (Placa)**: Reducido a **4 salidas** (luces).
3.  **Entradas Analógicas**: Reducido a **2 entradas** (Nivel de agua + Batería).
4.  **Salidas SSR de Potencia (Externas)**: Reducido a **2 salidas** (Bomba pileta + Calefacción).
5.  **Salidas MOSFET DC**: Reducido a **2 salidas** (Sirena + Auxiliar 12V).
6.  **Ultrasonido**: **2 pines** (Trigger + Echo).
7.  **Bus I2C**: **2 pines** (SDA, SCL) compartidos para pantallas y sensores.
8.  **Bus 1-Wire**: **1 pin** (múltiples sensores de temperatura DS18B20).
9.  **RS485**: **3 pines** (TX, RX, DE/RE para control de flujo).
10. **Ethernet**: **NO es posible** por falta de pines (el W5500 por SPI requiere al menos 6 pines dedicados).

*   **Total GPIOs requeridos**: **24 pines**.
*   **Pros**: Circuito más simple, menor costo de PCB, menor complejidad de código (lectura directa de registros de GPIO).
*   **Contras**: Muy limitado si en el futuro deseas expandir sensores; no permite Ethernet; perdimos el 60% de los canales solicitados.

---

### OPCIÓN B: Con Expansores de Puertos (Diseño Recomendado)

Utilizamos expansores de puertos digitales que se comunican por **I2C** o **SPI**. El chip más recomendado es el **MCP23017** (16 E/S digitales por I2C) o el **MCP23S17** (versión SPI, más rápida).

#### Arquitectura de Expansión Propuesta:
*   **Expansor 1 (MCP23017 - Dirección I2C 0x20)**: Configurado completamente como **Entradas**. Controla las **16 entradas digitales optoacopladas**. 
    *   *Nota de diseño*: Se conecta un pin de interrupción del MCP23017 a un GPIO del ESP32-S3 para avisar instantáneamente si hay cambios de estado en alguna entrada (evitando hacer polling continuo).
*   **Expansor 2 (MCP23017 - Dirección I2C 0x21)**: Configurado completamente como **Salidas**. Controla:
    *   Las 8 salidas SSR G3MB-202P.
    *   Las 4 salidas para SSR externo de potencia.
    *   Las 4 salidas MOSFET de potencia.
    *   *(Total: 16 salidas).*

#### Distribución de Pines en el ESP32-S3 (Con Expansores):
1.  **Bus I2C (SDA, SCL)**: **2 pines**. Conecta con ambos MCP23017, pantallas y sensores I2C.
2.  **Interrupciones de Entrada (INTA/INTB de MCP)**: **1 pin**. Avisa de cambios en las 16 entradas.
3.  **Entradas Analógicas**: **4 pines nativos** (conectados a los ADCs del ESP32-S3 para máxima velocidad y resolución).
4.  **Ultrasonido (JSN-SR04T)**: **2 pines nativos** (para medir tiempos de Echo con precisión de microsegundos).
5.  **Bus 1-Wire**: **1 pin nativo** (para sensores DS18B20).
6.  **RS485 Aislado**: **3 pines nativos** (UART dedicada + control de flujo DE/RE).
7.  **Ethernet (SPI W5500)**: **6 pines nativos** (MISO, MOSI, SCK, CS, RST, INT).
8.  **Pines de Configuración (Dip-Switch)**: **2 pines nativos** (opcionales para modo de firmware).

*   **Total GPIOs requeridos**: **21 pines nativos** en el ESP32-S3.
*   **Pros**: 
    *   Cumple al 100% con los requerimientos originales de E/S.
    *   Soporta Ethernet por hardware y RS485.
    *   Sobran GPIOs nativos en el ESP32-S3 para futuros usos.
    *   Los expansores MCP23017 actúan como "fusible" o barrera física; si hay una sobretensión extrema en una entrada o salida, se quema el expansor (un chip económico de reemplazar) y no el ESP32-S3.
*   **Contras**: Mayor costo del PCB y más líneas de ruteo; el firmware requiere inicializar y leer los expansores mediante librerías I2C.

---

## 3. Requerimientos de Protección Eléctrica (Detallados)

Independientemente de la opción elegida, definimos las siguientes protecciones mínimas para el esquemático:

### A. Entradas Digitales (16 Canales)
*   **Optoacopladores**: Aislamiento galvánico total entre el cable exterior del sensor y el microcontrolador/expansor.
*   **Filtros RC pasivos**: Una resistencia de serie y un capacitor cerámico (ej. 100nF) antes del LED del optoacoplador para mitigar picos inducidos y rebotes.
*   **Diodos de protección**: Diodos de protección contra polaridad inversa en los terminales de entrada de cada canal.

### B. Entradas Analógicas (4 Canales)
*   **Diodos Zener o TVS de 3.3V**: En paralelo con la entrada analógica para recortar voltajes mayores a 3.3V que puedan dañar los pines ADC.
*   **Capacitor de filtrado de ruido**: Capacitor electrolítico pequeño o de tantalio para suavizar lecturas inestables.

### C. Salidas de Potencia y MOSFETs
*   **Salidas SSR (G3MB-202P)**: Fusibles rápidos de protección en cada salida AC para evitar incendios si la carga entra en cortocircuito.
*   **Salidas MOSFET (DC)**: Diodos Flyback ultra-rápidos (ej. *1N4007* o *M7* en SMD) integrados en paralelo con la bornera de salida para proteger el MOSFET del pico inductivo al apagar solenoides, sirenas o motores.
*   **Aislamiento**: Driver buffer de compuerta (gate driver) o transistores BJT intermedios para asegurar que el MOSFET sature completamente a 5V o 12V y no recaliente.
