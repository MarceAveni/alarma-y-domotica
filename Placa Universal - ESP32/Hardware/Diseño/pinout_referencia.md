# Mapa de Pines (Pinout) de Referencia - Placa Universal ESP32-S3

Este documento es la **única fuente de verdad** para las conexiones físicas entre la placa de desarrollo **ESP32-S3 DevKitC-1** y los periféricos de la placa universal. Sirve tanto para el diseño del esquema electrónico como para las definiciones `#define` en los firmwares.

---

## 1. Resumen Gráfico de Asignación de Pines

| GPIO ESP32-S3 | Función | Tipo | Descripción / Restricciones |
| :---: | :---: | :---: | :--- |
| **GPIO 0** | BOOT / PRG | Entrada | Botón de Boot en DevKit (Strapping Pin - No usar para E/S) |
| **GPIO 1** | ANALOG_1 | Analógico (ADC1_CH0) | Entrada Analógica 1 (Apta para usar con WiFi activo) |
| **GPIO 2** | ANALOG_2 | Analógico (ADC1_CH1) | Entrada Analógica 2 (Apta para usar con WiFi activo) |
| **GPIO 3** | ANALOG_3 | Analógico (ADC1_CH2) | Entrada Analógica 3 (Apta para usar con WiFi activo) |
| **GPIO 4** | ANALOG_4 | Analógico (ADC1_CH3) | Entrada Analógica 4 (Apta para usar con WiFi activo) |
| **GPIO 5** | US_TRIG | Salida Digital | Trigger común para los 4 sensores ultrasónicos JSN-SR04T |
| **GPIO 6** | US_ECHO_1 | Entrada Digital | Echo del Sensor Ultrasónico 1 |
| **GPIO 7** | US_ECHO_2 | Entrada Digital | Echo del Sensor Ultrasónico 2 |
| **GPIO 8** | I2C_SDA | E/S Digital (I2C) | Bus I2C - Datos (con pull-up externo de 4.7k) |
| **GPIO 9** | I2C_SCL | Salida Digital (I2C) | Bus I2C - Reloj (con pull-up externo de 4.7k) |
| **GPIO 10** | MCP_INT | Entrada Digital | Interrupción activa en bajo de las entradas del MCP23017 |
| **GPIO 11** | SPI_MOSI | Salida Digital (SPI) | Bus SPI - Master Out Slave In |
| **GPIO 12** | SPI_MISO | Entrada Digital (SPI) | Bus SPI - Master In Slave Out |
| **GPIO 13** | SPI_SCLK | Salida Digital (SPI) | Bus SPI - Reloj de Serie |
| **GPIO 14** | ETH_CS | Salida Digital (SPI) | Chip Select para el módulo Ethernet W5500 |
| **GPIO 15** | US_ECHO_3 | Entrada Digital | Echo del Sensor Ultrasónico 3 |
| **GPIO 16** | US_ECHO_4 | Entrada Digital | Echo del Sensor Ultrasónico 4 |
| **GPIO 17** | ONE_WIRE | E/S Digital (1-Wire)| Bus de Temperatura (con pull-up de 4.7k) |
| **GPIO 18** | RS485_TX | Salida (UART2_TX) | Transmisión UART hacia el chip transceptor RS485 |
| **GPIO 19** | USB_D- | USB Nativo | Reservado para puerto USB-OTG de depuración/flasheo |
| **GPIO 20** | USB_D+ | USB Nativo | Reservado para puerto USB-OTG de depuración/flasheo |
| **GPIO 21** | RS485_RX | Entrada (UART2_RX) | Recepción UART desde el chip transceptor RS485 |
| **GPIO 38** | RS485_DE_RE | Salida Digital | Control de flujo dirección RS485 (1 = Transmitir, 0 = Recibir) |
| **GPIO 39** | ETH_RST | Salida Digital | Reset de hardware para el chip Ethernet W5500 |
| **GPIO 40** | ETH_INT | Entrada Digital | Interrupción de hardware del chip Ethernet W5500 |
| **GPIO 43** | UART0_TX | UART de Consola | Conectado al chip USB-to-UART del DevKit (Flasheo/Consola) |
| **GPIO 44** | UART0_RX | UART de Consola | Conectado al chip USB-to-UART del DevKit (Flasheo/Consola) |
| **GPIO 45** | Reservado | Strapping Pin | Deja flotante/LOW en arranque (Ajusta voltaje SPI Flash) |
| **GPIO 46** | Reservado | Strapping Pin | Deja flotante/LOW en arranque (Controla descarga de Boot) |
| **GPIO 47** | LIBRE | - | Disponible para expansiones en placa |
| **GPIO 48** | RGB_LED | Salida Digital | Conectado al LED RGB NeoPixel interno del DevKitC-1 |

---

## 2. Notas de Diseño Críticas para Hardware y Firmware

### ⚠️ Regla de Oro sobre las Entradas Analógicas:
Los pines analógicos asignados (**GPIO 1, 2, 3 y 4**) pertenecen al **ADC1** del ESP32-S3. 
* *Razón*: El ESP32-S3 tiene dos convertidores analógicos (ADC1 y ADC2). El ADC2 se desconecta por hardware internamente cuando la radio WiFi o Bluetooth está activa. Al usar canales del ADC1, podemos leer sensores analógicos en tiempo real sin interferir con las comunicaciones inalámbricas.

### 🔌 Divisores de Tensión obligatorios para Ultrasonidos (Echo):
Los sensores de ultrasonido JSN-SR04T se alimentan y operan a **5V**. Por lo tanto, los pines `US_ECHO_x` entregarán un pulso de 5V.
* *Solución*: En el PCB, coloca un divisor resistivo simple (ej. **1k Ohm en serie** y **2k Ohm a GND**) en cada una de las 4 líneas de Echo (**GPIO 6, 7, 15 y 16**) para reducir el pulso a 3.3V antes de entrar al microcontrolador.

### 🛡️ Pines de Arranque (Strapping Pins) Protegidos:
Evitamos conectar periféricos de entrada/salida que puedan forzar estados lógicos en los pines **GPIO 0, 45 y 46** durante el arranque, ya que esto podría impedir que el ESP32-S3 inicie correctamente o ponerlo en modo de flasheo permanente.

---

## 3. Código C++ de Cabecera Común (`pinout.h`)

Este fragmento de código puede copiarse directamente en la carpeta `/Librerias_Compartidas/` para ser incluido en los tres proyectos de firmware:

```cpp
#ifndef PINOUT_UNIVERSAL_H
#define PINOUT_UNIVERSAL_H

// --- BUSES DE COMUNICACIÓN ---
#define I2C_SDA_PIN      8
#define I2C_SCL_PIN      9
#define ONE_WIRE_PIN     17

// --- ENTRADAS ANALÓGICAS (ADC1) ---
#define ANALOG_1_PIN     1
#define ANALOG_2_PIN     2
#define ANALOG_3_PIN     3
#define ANALOG_4_PIN     4

// --- SENSORES DE ULTRASONIDO ---
#define US_TRIG_PIN      5
#define US_ECHO_1_PIN    6
#define US_ECHO_2_PIN    7
#define US_ECHO_3_PIN    15
#define US_ECHO_4_PIN    16

// --- ETHERNET SPI (W5500) ---
#define ETH_SPI_MOSI    11
#define ETH_SPI_MISO    12
#define ETH_SPI_SCLK    13
#define ETH_CS_PIN      14
#define ETH_RST_PIN     39
#define ETH_INT_PIN     40

// --- BUS INDUSTRIAL RS485 (UART2) ---
#define RS485_TX_PIN    18
#define RS485_RX_PIN    21
#define RS485_DE_RE_PIN 38

// --- EXPANSOR I2C (MCP23017) ---
#define MCP_INT_PIN     10

#endif // PINOUT_UNIVERSAL_H
```
