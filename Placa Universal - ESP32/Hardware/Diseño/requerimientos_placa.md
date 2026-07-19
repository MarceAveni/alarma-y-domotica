# Documento de Requerimientos - Placa Universal ESP32-S3

Este documento define las especificaciones técnicas, eléctricas y funcionales para el diseño del hardware de la Placa Universal basada en el **ESP32-S3**. 

---

## 1. Resumen de Requerimientos de Entrada/Salida y Protocolos

El diseño se centra en una **arquitectura basada en expansores de puertos (Opción B)** para garantizar la disponibilidad de pines y la escalabilidad del sistema. 

Para reducir el tamaño de la PCB, simplificar el ruteo y aumentar la seguridad eléctrica, **no se integrarán relés mecánicos ni de estado sólido (SSR) directamente en la placa**. En su lugar, se dispondrán salidas a nivel lógico (cabezales de pines o borneras pequeñas) para conectar módulos externos.

### Entradas/Salidas y Conectores:
*   **16 Entradas Digitales**: Aisladas mediante optoacopladores para cables largos (sensores PIR, magnéticos de alarmas, pulsadores).
*   **6 Salidas para Relés de Estado Sólido (SSR) Externos**:
    *   Salidas lógicas de control a borneras o pines de conexión.
    *   Pensadas para conectar un módulo SSR externo comercial de 4 canales (tipo G3MB-202P) + 2 canales adicionales para relés SSR de potencia industriales externos (ej. 60A / 24-380VAC).
*   **4 Salidas MOSFET de Potencia en placa**: Para conmutar cargas DC de 12V de forma directa (Sirena de alarma, tiras LED, electroválvulas, etc.).
*   **4 Entradas Analógicas Nativas**: Para mediciones de sensores analógicos directos (ej. nivel de agua por presión/corriente, monitoreo de tensión de baterías o sensores de luz).
*   **4 Puertos para Sensores Ultrasonido (JSN-SR04T)**: Conexión mediante 5 pines nativos (1 pin de Trigger común y 4 pines de Echo dedicados).
*   **Buses de Comunicación**:
    *   **I2C**: Para pantallas de configuración (OLED), sensores de clima (BME280) y periféricos de expansión.
    *   **SPI**: Reservado para comunicación rápida (pantallas TFT, lector RFID o periféricos).
    *   **1-Wire**: Bornera con pull-up para sensores de temperatura DS18B20 en cascada.
    *   **RS485 Aislado**: Bus industrial con transceptor aislado galvánicamente para comunicación de larga distancia (Modbus).
    *   **Ethernet**: Conexión cableada redundante mediante módulo SPI **W5500**.

---

## 2. Arquitectura de Expansión de Puertos y Distribución de Pines

Utilizamos dos expansores de puertos **MCP23017** conectados al bus **I2C**, lo que permite manejar las 16 entradas y las 10 salidas digitales consumiendo únicamente 2 pines de control del ESP32-S3 (más el pin de interrupción).

### A. Mapeo de los Expansores (MCP23017)
*   **Expansor 1 (MCP23017 - Dirección I2C 0x20)**:
    *   **GPIO A0 a B7 (16 pines)**: Conectados a las **16 entradas digitales optoacopladas**.
    *   *Interrupción*: Pin INTA/INTB conectado al ESP32-S3 para detectar flancos de subida/bajada de forma inmediata sin sobrecargar la CPU haciendo polling.
*   **Expansor 2 (MCP23017 - Dirección I2C 0x21)**:
    *   **GPIO A0 a A5 (6 pines)**: Conectados a las **6 salidas de control de SSR externos**.
    *   **GPIO A6 a B1 (4 pines)**: Conectados a las compuertas de los **4 MOSFETs de potencia DC en placa**.
    *   **GPIO B2 a B7 (6 pines)**: **Pines Libres** para futuras ampliaciones (LEDs de estado, zumbadores de sistema, configuración, etc.).

### B. Distribución de Pines del ESP32-S3
1.  **Bus I2C (SDA, SCL)**: **2 pines**. Conexión de ambos MCP23017 y sensores locales.
2.  **Interrupción de Entradas (MCP1_INT)**: **1 pin**.
3.  **Entradas Analógicas**: **4 pines nativos** (conectados a los canales de ADC del ESP32-S3).
4.  **4 Sensores por Ultrasonido (JSN-SR04T)**: **5 pines nativos** (1 Trigger común + 4 Echo independientes).
5.  **Bus 1-Wire**: **1 pin nativo** (para sensores de temperatura DS18B20).
6.  **RS485 Aislado**: **3 pines nativos** (TX, RX del UART dedicado + pin DE/RE para el control de flujo).
7.  **Ethernet (Módulo SPI W5500)**: **6 pines nativos** (MOSI, MISO, SCK, CS, RST, INT).

*   **Total de GPIOs del ESP32-S3 en uso**: **22 pines nativos**.
*   **Pines Libres en el DevKit**: Quedan libres alrededor de 6 a 8 GPIOs nativos para propósitos adicionales.

---

## 3. Requerimientos de Protección Eléctrica (Detallados)

Al no tener relés de alterna (AC) dentro de la placa, el diseño del PCB es **100% de baja tensión (12V / 5V / 3.3V)**, lo que elimina el peligro de arcos de AC y reduce drásticamente el ruido inductivo en la placa principal.

### A. Entradas Digitales (16 Canales)
*   **Optoacopladores (PC817)**: Aislamiento total de las líneas físicas externas.
*   **Filtros RC pasivos**: Resistencia limitadora de corriente y capacitor cerámico de 100nF para suprimir transitorios causados por inducción en cables largos de sensores de alarma.
*   **Diodos contra Polaridad Inversa**: Diodo en paralelo inverso en el LED del optoacoplador para proteger el canal en caso de error en el cableado externo.

### B. Entradas Analógicas (4 Canales)
*   **Diodos Zener/TVS de 3.3V**: En paralelo para limitar la tensión de entrada y proteger el convertidor ADC del ESP32-S3.
*   **Filtro RC**: Estabiliza la medición de señales analógicas.

### C. Salidas de Control de SSR Externos (6 Canales)
*   **Buffer de Corriente (ULN2803)**: Las 6 líneas del expansor controlan un integrado ULN2803 que actúa como buffer (drenando la corriente de control de los relés a masa), evitando exigir de corriente al MCP23017.
*   **Conectores dedicados**: Exponer las salidas con pines de señal, alimentación (5V) y masa (GND).

### D. Salidas MOSFET de Potencia DC (4 Canales)
*   **MOSFETs Logic-Level (ej. AOD4184 en SMD o IRLZ44N en TH)**: Controlados desde el ULN2803 o directamente desde el expansor usando resistencias de compuerta y pull-down de 10k Ohm para evitar activaciones aleatorias durante el arranque.
*   **Diodos Schottky Flyback**: Diodo SMD (ej. SS34) en paralelo con los bornes de salida para proteger el MOSFET del pico inductivo de solenoides o sirenas.
