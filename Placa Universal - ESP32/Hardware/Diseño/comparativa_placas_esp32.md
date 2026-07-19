# Comparativa de Módulos ESP32 - Placa Universal

Este documento evalúa los diferentes modelos de microcontroladores de la familia ESP32 para determinar cuál es la opción más idónea para nuestra Placa Universal, considerando:
1.  **Capacidades técnicas** (pines libres, USB, ADC).
2.  **Disponibilidad local en Argentina**.
3.  **Costos relativos** (tanto en formato Placa de Desarrollo/DevKit como en Módulo de montaje superficial).
4.  **Complejidad de diseño e integración**.

---

## 1. Tabla Comparativa de Modelos

| Característica | ESP32 Clásico (WROOM-32D/U/E) | ESP32-S3 (WROOM-1/2) | ESP32-C3 (MINI-1/WROOM-02) |
| :--- | :---: | :---: | :---: |
| **Arquitectura** | Xtensa LX6 Dual-Core 32-bit | Xtensa LX7 Dual-Core 32-bit | RISC-V Single-Core 32-bit |
| **Frecuencia Máx.** | 240 MHz | 240 MHz | 160 MHz |
| **SRAM Interna** | 320 KB | 512 KB | 400 KB |
| **GPIOs Físicos** | 38 | 45 | 22 |
| **GPIOs Libres en DevKit**| ~26 a 28 | ~30 a 34 | ~15 |
| **USB Nativo (CDC/OTG)** | ❌ No (Requiere CH340/CP2102) |  Sí (Soporta Flash y JTAG) |  Sí (Soporta CDC de programación) |
| **Bluetooth** | v4.2 BR/EDR y BLE | v5.0 BLE (Mesh) | v5.0 BLE |
| **Pines Input-Only (GPI)**| 4 pines (GPIO 34-39, sin pull-ups) | ❌ Ninguno (Todos son bidireccionales)| ❌ Ninguno |
| **Disponibilidad en Arg.**| **Excelente (Muy alta)** | **Media-Alta** | **Media** |
| **Precio Relativo (DevKit)**| 💲 Económico (~$6.000-$8.000 ARS) | 💲💲 Medio-Alto (~$12.000-$18.000 ARS) | 💲 Muy Económico (~$5.000-$7.000 ARS)|

---

## 2. Análisis Detallado por Modelo

### A. ESP32 Clásico (ESP32-WROOM-32D / WROOM-32E)
Es el estándar del mercado desde hace años. 
*   **Pines y Recursos**: Cuenta con unos 26-28 GPIOs disponibles en placas de desarrollo (como el *NodeMCU-32S* o *ESP32 DevKit V1*). Posee 4 pines que son exclusivamente de entrada (**GPIO 34, 35, 36 y 39**), los cuales son perfectos para nuestras **entradas analógicas ADC1**, ya que no interfieren con las salidas.
*   **Disponibilidad en Argentina**: Máxima. Se consigue en cualquier casa de electrónica local (PatagoniaTec, Electrocomponentes, etc.) y MercadoLibre con entrega inmediata. Si se quema una placa, se reemplaza en el día.
*   **Desventaja**: No tiene USB nativo. Si diseñas la PCB para soldar el módulo WROOM de montaje superficial, tendrás que agregar el circuito del chip USB-a-UART (CH340C o CP2102) y su lógica de auto-reset, encareciendo el diseño. Si usas la placa DevKit como módulo insertable (tipo shield), esta desventaja desaparece.

### B. ESP32-S3 (ESP32-S3-WROOM-1)
Es el chip moderno de Espressif con vector de instrucciones para IA.
*   **Pines y Recursos**: Tiene más de 30 GPIOs útiles, todos bidireccionales con pull-ups/pull-downs internos configurables. 
*   **USB Nativo**: Esta es su mayor ventaja si diseñas la PCB desde el módulo SMD. El ESP32-S3 tiene soporte USB integrado en los pines GPIO 19 (D-) y 20 (D+). Solo necesitas soldar un puerto USB-C directo a esos pines y el chip se programa y depura solo, ahorrando componentes en tu BOM.
*   **Disponibilidad en Argentina**: Buena en tiendas especializadas en línea, aunque menos común en locales de mostrador tradicionales. Su costo es el más elevado de los tres.

### C. ESP32-C3 (ESP32-C3-MINI-1)
Basado en la nueva arquitectura abierta RISC-V, ideal para bajo consumo.
*   **Pines y Recursos**: Muy limitado. Solo dispone de unos 15 GPIOs útiles en formato DevKit.
*   **Incompatibilidad con nuestro diseño**:
    *   No tiene pines suficientes para manejar Ethernet (SPI de 6 pines) + RS485 (3 pines) + I2C (2 pines) + Ultrasonidos (5 pines) + 1-Wire (1 pin) + Analógicos (4 pines).
    *   Para usarlo, tendríamos que pasar funciones críticas como los receptores de Echo de los ultrasonidos (que miden tiempos en microsegundos) a expansores I2C, lo que es inviable debido a la latencia de bus I2C (provocaría un error de medición enorme en la distancia).
*   **Ideal para**: Nódulos satélite simples (ej. reemplazo del ESP-01), pero no para la placa concentradora universal.

---

## 3. Impacto del Método de Integración en el PCB

Al diseñar la PCB en Altium, tienes dos caminos que definen el costo y la elección del chip:

### Opción 1: Placa DevKit como Módulo Insertable (Recomendado para prototipos/Hogar)
Diseñas la placa principal con tiras de pines hembra (headers) donde pinchas una placa de desarrollo comercial (DevKit) completa.
*   **Ventajas**:
    *   Fácil reemplazo en caso de fallas (se saca y se pone otro).
    *   El circuito USB-UART y regulador de 3.3V auxiliar ya vienen resueltos en el DevKit.
    *   **Mejor opción**: **ESP32 Clásico DevKitC (38 pines)** o **ESP32-S3 DevKitC-1**.
*   **Desventajas**: Mayor altura física en el gabinete.

### Opción 2: Módulo SMD Soldado directamente a la PCB (Diseño Profesional/Industrial)
Diseñas la PCB integrando directamente el módulo metálico (ej. ESP32-WROOM-32E o ESP32-S3-WROOM-1).
*   **Ventajas**: Placa extremadamente delgada, estética industrial, menor costo por unidad si se fabrican en cantidad.
*   **Mejor opción**: **ESP32-S3-WROOM-1**. Al tener USB nativo, te ahorras poner el chip CH340 y sus componentes pasivos asociados, simplificando enormemente el ruteo en Altium.

---

## 4. Conclusión y Recomendación

1.  **Si vas a usar placas DevKit insertables (Shield)**:
    *   **Usa el ESP32 Clásico DevKit de 38 pines**. Es el más barato, hay stock de sobra en Argentina, y tiene pines de sobra para cubrir los 22 GPIOs nativos requeridos en la Opción B.
2.  **Si vas a diseñar con módulo SMD soldado**:
    *   **Usa el módulo ESP32-S3-WROOM-1**. La facilidad de ruteo que te da conectar el conector USB-C directo al chip (GPIO 19 y 20) compensa con creces el costo extra del chip frente al clásico.
3.  **Descarte**: El **ESP32-C3** queda descartado para esta placa concentradora por la falta de pines para los ultrasonidos directos y el módulo Ethernet.
