# Recomendaciones de Diseño de PCB y Esquemas - Placa Universal ESP32-S3

Este documento proporciona pautas de diseño electrónico y de circuito impreso (PCB) para la Placa Universal. Su objetivo es garantizar la estabilidad del sistema, mitigar el ruido electromagnético de relés y sirenas, y proteger el ESP32-S3 de transitorios en cables largos.

Al optar por **conectar módulos SSR de manera externa (Opción B)**, el PCB principal queda libre de alta tensión (220VAC), lo que reduce drásticamente el tamaño físico de la placa, abarata los costos de fabricación y elimina el ruido de conmutación de corriente alterna cerca del microcontrolador.

---

## 1. Topología del PCB y Zonas de Aislamiento

Aunque la placa ahora es 100% de baja tensión continua (DC), sigue siendo fundamental separar las líneas de alta corriente (12V para sirenas y actuadores) de las señales de control de alta velocidad (ESP32-S3, buses I2C/SPI).

```text
+-------------------------------------------------------------------+
|               ZONA DE CONTROL (Baja Potencia)                     |
|  [ESP32-S3]     [MCP23017]     [Regulador LDO 3.3V]    [I2C/SPI]  |
+-------------------------------------------------------------------+
|               ZONA DE AISLAMIENTO (Barrera)                       |
|  [Optoacopladores PC817]              [Drivers ULN2803/MOSFETs]   |
+-------------------------------------------------------------------+
|               ZONA DE POTENCIA DC y CONECTORES                    |
|  [Mosfets Sirena/DC]   [Conectores SSR/Sensores]   [Regulador Buck]|
+-------------------------------------------------------------------+
```

### Reglas de Layout (Ruteo):
1.  **Planos de Masa Separados**: 
    *   `GND_DF` (Masa digital de control, conectada al ESP32-S3 y expansores).
    *   `GND_PWR` (Masa de potencia de 12V para sirenas, electroválvulas y regulador Buck).
    *   Ambos planos deben unirse en **un solo punto** (Star Ground) cerca del capacitor de entrada de 12V, asegurando que las corrientes de retorno de la sirena no fluyan por debajo del ESP32-S3.
2.  **Ancho de Pistas de Potencia**:
    *   Pistas de masa y alimentación de 12V (hacia los MOSFETs): Mínimo 2 mm (o usar polígonos de cobre) para soportar de forma segura corrientes pico de sirena de hasta 5A.

---

## 2. Circuitos Propuestos por Sección

### A. Alimentación y Regulación
*   **Regulador Buck (12V -> 5V)**: Usar un regulador de alta eficiencia como el **MP2315** o **AP63205** para alimentar las entradas de los módulos SSR externos, sensores ultrasónicos y la línea del regulador LDO de 3.3V.
*   **Regulador LDO (5V -> 3.3V)**: Usar un chip lineal robusto como el **AP2112K-3.3** o **AMS1117-3.3** en SOT-223 para el ESP32-S3 y sensores locales de 3.3V.

### B. Entradas Digitales Optoacopladas (16 Canales)
Para conmutar las entradas del MCP23017 de manera segura usando 12V o 5V externos:

```text
(Entrada exterior +12V) ----[ R1: 1k Ohm ]----+-----------------+
                                              |                 |
                                           [ Diodo ]        [ Led Opto ] (PC817)
                                          (Inverso/1N4148)      |
(Retorno Exterior GND) -----------------------+-----------------+

----------------------- Aislamiento Físico -----------------------

(VCC 3.3V) ----[ R2: 10k Ohm ]----+----> Pin Entrada MCP23017
                                  |
                           [ Colector Opto ] (PC817)
                                  |
                           [ Emisor Opto ]
                                  |
(GND Digital) --------------------+----+----> Capacitor Filtro (10nF)
```
*   *Función del Diodo 1N4148*: Protege el LED del optoacoplador en caso de conexión inversa accidental de los cables del sensor.
*   *Capacitor de 10nF*: Junto con R2, forma un filtro paso bajo de hardware que elimina rebotes de alta frecuencia y ruidos inducidos antes de llegar al MCP23017.

### C. Conexiones y Activación de SSRs Externos (6 Canales)
Al no integrar los relés de alterna en la PCB, dejamos borneras o pines de conexión que transportan la alimentación y la señal lógica de control. 

#### Métodos de Activación según Tipo de Módulo:
1.  **Módulos Comerciales SSR (Activos en Bajo / Active-Low)**:
    *   Muchos módulos comerciales de 4 canales G3MB-202P se activan drenando corriente a masa (GND).
    *   **Propuesta**: Conectar las 6 salidas del MCP23017 a un chip driver Darlington **ULN2803** (o **ULN2003**). Las salidas del ULN actúan como interruptores a masa (sink) capaces de manejar hasta 500mA por canal sin sobrecargar el expansor I2C.
2.  **Módulos Comerciales SSR (Activos en Alto / Active-High)**:
    *   Si el módulo externo requiere una señal de 5V activa para encender, las salidas del MCP23017 (que funcionan a 3.3V) se pueden pasar por un desplazador de nivel (ej. chip **74HCT244** o transistores individuales).

#### Diseño del Conector (Ejemplo de Bornera / Pin Header):
*   **Conector 1 (Módulo 4 Canales SSR)**: Exponer un conector con 6 pines: `[ VCC 5V, GND, In1, In2, In3, In4 ]`.
*   **Conector 2 y 3 (SSRs de potencia individuales)**: Dos conectores de 2 pines (o borneras de paso 3.81mm): `[ VCC 5V/12V, Señal Control ]`.

### D. Salidas MOSFET de Potencia DC en Placa (4 Canales)
Para controlar sirenas de 12V y reflectores DC de alta potencia:
*   Usar MOSFETs de canal N con nivel lógico de compuerta (Logic-Level Gate), como el **IRLZ44N** (TH) o **AOD4184** (SMD).
*   **Circuito de disparo**:
    *   Resistencia en serie con la compuerta (Gate): **100 Ohm** para limitar la corriente transitoria de carga de la compuerta.
    *   Resistencia de Pull-Down en la compuerta: **10k Ohm** a GND para asegurar que el MOSFET permanezca apagado durante el arranque del microcontrolador.
    *   **Diodo Schottky flyback (paralelo a la bornera de salida)**: Indispensable (ej. *SS34* o *1N5822*) para derivar el pico inductivo inverso al apagar sirenas o solenoides.

---

## 3. Interfaces de Comunicación y Sensores

### A. Bus I2C y 1-Wire (Protección contra descargas)
Los cables que van hacia sensores externos (como sensores de temperatura DS18B20 o displays lejanos) pueden actuar como antenas y acumular electricidad estática.
*   Colocar arreglos de diodos de protección ESD tipo **USBLC6-2SC6** en las líneas de SDA, SCL y la línea de datos de 1-Wire. Esto derivará cualquier descarga estática a tierra antes de que alcance el ESP32-S3.
*   **Resistencias de Pull-Up**:
    *   I2C: Resistencias de **4.7k Ohm** a 3.3V.
    *   1-Wire: Resistencia de **4.7k Ohm** (o **2.2k Ohm** si los cables de los sensores de temperatura son muy largos) a 3.3V.

### B. RS485 Aislado Galvánicamente
Para una comunicación RS485 robusta e industrial:
*   Usar un transceptor RS485 aislado como el **ADM2483** (o un transceptor clásico **MAX485** acoplado con optoacopladores rápidos de alta velocidad y un convertidor DC-DC aislado de 5V a 5V como el **B0505S-1W**).
*   Esto asegura que fallas eléctricas o diferencias de potencial de tierra entre la placa principal y los módulos remotos RS485 no quemen el circuito.

### C. Sensores por Ultrasonido (4 Canales - JSN-SR04T)
*   **Análisis de Interferencia Acústica (Crosstalk)**:
    Al disparar los 4 sensores simultáneamente usando un Trigger común, si están instalados en recipientes separados (ej. Tanque de agua de reserva, cisterna bajo tierra, piscina y depósito de agua de riego), **no habrá interferencia**. 
    *   *Si existiese riesgo de interferencia*, una alternativa elegante de hardware es pasar la señal de Trigger a través de una compuerta lógica demultiplexora (ej. **74HC138** o **74HC125**) controlada por el MCP23017 para activar selectivamente el Trigger del sensor que se desea medir en ese instante, usando un único pin de Trigger nativo de alta precisión del ESP32-S3.
*   **Adaptación de Voltaje**: El JSN-SR04T funciona a **5V**. La señal de Echo que retorna del sensor es de 5V. El ESP32-S3 solo tolera **3.3V**.
    *   *Solución*: Colocar un divisor de tensión resistivo (ej. **1k Ohm** en serie y **2k Ohm** a GND) en cada línea de Echo antes de entrar al GPIO del ESP32-S3, reduciendo la señal de 5V a 3.3V seguros.

---

## 4. Conectores Recomendados
*   Para salidas de potencia DC (MOSFETs, Alimentación): Borneras de tornillo de **paso 5.08 mm**.
*   Para las salidas de control SSR, entradas analógicas y sensores (1-Wire, Ultrasonido): Borneras de tornillo de **paso 3.81 mm** o conectores tipo JST XH2.54 para conexiones rápidas de sensores.
*   Para expansión/I2C/SPI: Conectores tipo pin header macho/hembra de **paso 2.54 mm** estándar.

---

## 5. Componentes de Soporte Mínimos para el ESP32-S3-WROOM-1

Para garantizar que el módulo inicie correctamente, sea fácil de programar y funcione de forma segura ante ruidos y descargas, debes incluir los siguientes componentes periféricos asociados al módulo en tu esquema de Altium:

### A. Circuito de Alimentación y Desacoplamiento (Pin 3V3 y GND)
El ESP32-S3 consume picos de corriente elevados (hasta 350-500mA) durante la transmisión de WiFi.
*   **Capacitor de desacoplamiento de alta capacidad**: Coloca un capacitor de tántalo o electrolítico SMD de **22 µF** (o 10µF) en paralelo con un capacitor cerámico de **0.1 µF (100nF)**.
*   *Ubicación*: Deben colocarse **lo más cerca posible** del pin `3V3` (Pin 2) y el pin `GND` (Pin 1) del módulo, con pistas anchas de alimentación.

### B. Circuito de Reset / Habilitación (Pin EN - Pin 3)
Para asegurar que el microcontrolador inicie de forma limpia una vez que la tensión de 3.3V se ha estabilizado:
*   **Resistencia de Pull-Up**: Resistencia de **10 kΩ** conectada a 3.3V.
*   **Capacitor de retardo**: Capacitor de **1 µF** conectado entre el pin `EN` y `GND`. Esto crea un retardo RC de inicio (aprox. 10 ms).
*   **Pulsador de Reset**: Puedes añadir un pulsador táctil en paralelo con el capacitor de 1µF para permitir un reinicio manual de la placa.

### C. Circuito de Programación por USB Nativo (Pines GPIO 19 y 20)
Dado que usaremos la programación nativa del ESP32-S3 mediante USB:
*   **Resistencias de protección en serie**: Conecta el pin `GPIO 19` (D-) y `GPIO 20` (D+) a las líneas de datos correspondientes del conector USB-C usando resistencias de **0 Ohm** o **22 Ohm** en serie (ej. encapsulado 0603). Sirven como fusibles de protección y para adaptación de impedancia de las pistas de alta velocidad.
*   **Resistencias del conector USB-C (CC1 y CC2)**: Es obligatorio colocar dos resistencias de **5.1 kΩ** conectadas individualmente desde los pines `CC1` y `CC2` del conector USB-C a `GND`. Sin estas resistencias, los cargadores modernos USB-C y los puertos de las notebooks no detectarán la placa y no le suministrarán energía (5V).
*   **Protección ESD**: Coloca un array de diodos de protección TVS como el **USBLC6-2SC6** en las líneas de datos `D+` y `D-` del USB-C.

### D. Circuito del Botón de Boot (GPIO 0 - Pin 27)
Aunque el USB nativo usualmente puede forzar el modo de programación automáticamente por software (mediante señales DTR/RTS virtuales), es altamente recomendable colocar un método físico de respaldo:
*   Coloca una resistencia de **10 kΩ** de pull-up conectada a 3.3V (aunque el chip tiene pull-up interna, asegura estabilidad ante ruido).
*   Coloca un pulsador táctil que conecte `GPIO 0` a `GND` a través de una resistencia pequeña de protección (ej. **470 Ohm**). Si mantienes presionado este botón al encender o resetear la placa, entrará obligatoriamente en modo de flasheo por hardware.

### E. Regla de Ruteo de la Antena (Keepout en Altium)
Si usas el modelo con antena de pista integrada (`ESP32-S3-WROOM-1`):
*   El módulo debe colocarse de manera que **la antena sobresalga del borde de la placa principal (overhang)**, o bien debes declarar una zona de **Keepout (exclusión de cobre)** en todas las capas del PCB directamente debajo y alrededor del área de la antena (los 6.0 mm superiores del módulo). No coloques planos de tierra, pistas de señales ni componentes en esta área, de lo contrario degradarás severamente el alcance de la radio.

