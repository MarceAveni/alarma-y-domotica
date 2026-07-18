# Recomendaciones de Diseño de PCB y Esquemas - Placa Universal ESP32-S3

Este documento proporciona pautas de diseño electrónico y diseño de circuito impreso (PCB) para la Placa Universal. Su objetivo es garantizar la estabilidad del sistema, mitigar el ruido electromagnético de relés y sirenas, y proteger el ESP32-S3 de transitorios en cables largos.

---

## 1. Topología del PCB y Zonas de Aislamiento

Para evitar que el ruido de conmutación de alterna (AC) o de alta corriente continua (DC) afecte la estabilidad del microcontrolador, el PCB debe dividirse físicamente en **tres zonas claras**:

```text
+-------------------------------------------------------------------+
|               ZONA DE CONTROL (Baja Potencia)                     |
|  [ESP32-S3]     [MCP23017]     [Regulador LDO 3.3V]    [I2C/SPI]  |
+-------------------------------------------------------------------+
|               ZONA DE AISLAMIENTO (Barrera)                       |
|  [Optoacopladores PC817]   [Drivers ULN2803]   [Opto-Triacs/SSR]  |
+-------------------------------------------------------------------+
|               ZONA DE POTENCIA (Alta Potencia)                    |
|  [Relés SSR G3MB-202P]    [MOSFETs DC 12V]    [Regulador Buck 12V]|
+-------------------------------------------------------------------+
```

### Reglas de Layout (Ruteo):
1.  **Planos de Masa Separados**: 
    *   `GND_DF` (Masa digital de control, conectada al ESP32-S3 y expansores).
    *   `GND_PWR` (Masa de potencia de 12V para sirenas, electroválvulas y bobinas/SSR).
    *   Ambos planos deben unirse en **un solo punto** (Star Ground) cerca del capacitor de entrada de la fuente de alimentación, o mantenerse completamente aislados si se usan optoacopladores para todo el retorno.
2.  **Distancia de Fuga (Creepage) y Despeje (Clearance)**:
    *   Mantener una separación mínima de **3 mm** entre cualquier pista de 220VAC (salidas de relés SSR) y cualquier pista de baja tensión (DC).
    *   Se recomienda hacer **ranuras de aislamiento (slots/cortes en el PCB)** debajo de los relés SSR (entre los pines de AC y de control) para evitar arcos eléctricos en ambientes húmedos.
3.  **Ancho de Pistas de Potencia**:
    *   Pistas de 220VAC (SSR G3MB-202P): Mínimo 1.5 mm (alrededor de 60 mil) para soportar 2A de corriente con un aumento mínimo de temperatura.
    *   Pistas de masa y alimentación de 12V (MOSFETs): Mínimo 2 mm (o usar polígonos de cobre) si se maneja sirena o reflectores DC de hasta 5A.

---

## 2. Circuitos Propuestos por Sección

### A. Alimentación y Regulación
*   **Regulador Buck (12V -> 5V)**: Usar un chip monolítico conmutado como el **MP2315** o **AP63205** (más moderno y estable). Requiere bobinas de montaje superficial (SMD) blindadas y capacitores cerámicos de entrada/salida de muy bajo ESR colocados lo más cerca posible del chip.
*   **Regulador LDO (5V -> 3.3V)**: Usar el **AP2112K-3.3** o **AMS1117-3.3** en encapsulado SOT-223. Añadir capacitores de tántalo o cerámicos de 10µF tanto en la entrada como en la salida para evitar oscilaciones.

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

### C. Activación de SSRs en Placa y Externos
El expansor MCP23017 tiene un límite de corriente total en su encapsulado. No se deben activar los relés SSR directamente desde sus pines.
*   **Propuesta**: Usar un array de transistores Darlington **ULN2803** (8 canales en un integrado) para controlar los 8 relés SSR **G3MB-202P**.
    *   El ULN2803 maneja la corriente de los LEDs de control del SSR (aprox 15mA por canal) hacia masa (sink) sin cargar al MCP23017.
*   **Salidas para SSR externos de Potencia**: 
    *   Dado que los SSR industriales externos (de 60A) requieren corrientes de control de hasta 20-30mA a 5V/12VDC, utiliza canales adicionales del ULN2803 o transistores MOSFET independientes (ej. **2N7002**).

### D. Salidas MOSFET de Potencia DC (4 Canales)
Para controlar sirenas de 12V y reflectores DC de alta potencia:
*   Usar MOSFETs de canal N con nivel lógico de compuerta (Logic-Level Gate), como el **IRLZ44N** (TH) o **AOD4184** (SMD).
*   **Circuito de disparo**:
    *   Resistencia en serie con la compuerta (Gate): **100 Ohm** para limitar la corriente transitoria de carga de la capacitancia de compuerta.
    *   Resistencia de Pull-Down en la compuerta: **10k Ohm** a GND para asegurar que el MOSFET permanezca apagado durante el encendido y reinicio del microcontrolador.
    *   **Diodo Schottky flyback (paralelo a la bornera de salida)**: Indispensable (ej. *SS34* o *1N5822*) para derivar el pico inductivo inverso de sirenas electromecánicas o solenoides.

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
El requerimiento modificado pide 5 pines nativos del ESP32-S3 (1 Trigger común, 4 Echos independientes).
*   **Análisis de Interferencia Acústica (Crosstalk)**:
    Al disparar los 4 sensores simultáneamente usando un Trigger común, si están instalados en recipientes separados (ej. Tanque de agua de reserva, cisterna bajo tierra, piscina y depósito de agua de riego), **no habrá interferencia**. 
    *   *Si existiese riesgo de interferencia*, una alternativa elegante de hardware es pasar la señal de Trigger a través de una compuerta lógica demultiplexora (ej. **74HC138** o **74HC125**) controlada por el MCP23017 para activar selectivamente el Trigger del sensor que se desea medir en ese instante, usando un único pin de Trigger nativo de alta precisión del ESP32-S3.
*   **Adaptación de Voltaje**: El JSN-SR04T funciona a **5V**. La señal de Echo que retorna del sensor es de 5V. El ESP32-S3 solo tolera **3.3V**.
    *   *Solución*: Colocar un divisor de tensión resistivo (ej. **1k Ohm** en serie y **2k Ohm** a GND) en cada línea de Echo antes de entrar al GPIO del ESP32-S3, reduciendo la señal de 5V a 3.3V seguros.

---

## 4. Conectores Recomendados
*   Para entradas y salidas de potencia (Relés, MOSFETs, Alimentación): Borneras de tornillo de **paso 5.08 mm** de alta calidad.
*   Para entradas analógicas y sensores (1-Wire, Ultrasonido): Borneras de tornillo de **paso 3.81 mm** o conectores tipo XH2.54 para conexiones rápidas de sensores.
*   Para expansión/I2C/SPI: Conectores tipo pin header macho/hembra de **paso 2.54 mm** estándar.
