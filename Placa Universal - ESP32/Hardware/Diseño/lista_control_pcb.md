# Lista de Control (Checklist) para el Diseño del PCB

Usa esta lista en Altium para verificar cada circuito y evitar errores de hardware clásicos antes de mandar a fabricar las placas. Puedes marcar las casillas colocando una `x` entre los corchetes (ej: `[x]`).

---

## 1. Módulo ESP32-S3 y Configuración de Arranque
- [ ] **Huella de Altium**: ¿Se utilizó el footprint para el `ESP32-S3-WROOM-1` (largo, 25.5 mm)? *Esto permitirá soldar tanto la versión con antena integrada como la versión -1U de antena externa.*
- [ ] **Exclusión de Cobre (Antena Keepout)**: ¿El área de 6.0 mm superiores del módulo (antena) sobresale del borde de la placa, o está libre de planos de masa y pistas en todas las capas del PCB?
- [ ] **Desacoplamiento de alimentación**: ¿Se colocaron un capacitor electrolítico/tántalo de **22 µF** en paralelo con uno cerámico de **0.1 µF (100nF)** pegados físicamente a los pines `3V3` y `GND` del módulo?
- [ ] **Circuito de Reset (EN)**: ¿Tiene una resistencia de **10 kΩ** a 3.3V y un capacitor de **1 µF** a GND? ¿Se colocó el pulsador en paralelo con el capacitor?
- [ ] **Circuito de Boot (GPIO 0)**: ¿Tiene su pull-up de **10 kΩ** a 3.3V y un pulsador a GND pasando por una resistencia protectora de **470 Ohm**?
- [ ] **Pines de Arranque (Strapping Pins)**: ¿Están los pines `GPIO 45` y `GPIO 46` libres de pull-ups externas que puedan impedir que el micro arranque?
- [ ] **Acceso a Programación USB Nativo**: 
    - [ ] ¿Se conectaron resistencias en serie de **22 Ohm** (o 0 Ohm) en las líneas `D-` (GPIO 19) y `D+` (GPIO 20) del USB-C?
    - [ ] ¿Se agregaron resistencias de **5.1 kΩ** independientes conectadas de `CC1` y `CC2` del conector USB-C hacia `GND`?
    - [ ] ¿Se agregó el integrado de protección ESD en las líneas del USB-C?

---

## 2. Bloque de Alimentación
- [ ] **Protección de Entrada 12V**: ¿Tiene un fusible reseteable **PTC (2A/3A)** y diodos TVS o Varistores para proteger contra sobretensiones transitorias?
- [ ] **Protección contra Inversión**: ¿Se incluyó un diodo Schottky o un transistor MOSFET de canal P para proteger el circuito ante conexión de fuente invertida?
- [ ] **Regulador Buck (12V -> 5V)**: ¿Los capacitores de filtrado y la bobina blindada están ruteados con pistas cortas y anchas de acuerdo al datasheet del integrado regulador conmutado?
- [ ] **Regulador LDO (5V -> 3.3V)**: ¿Tiene capacitores cerámicos de tántalo o bajo ESR a la entrada y a la salida para evitar oscilaciones?

---

## 3. Entradas Digitales (16 Canales)
- [ ] **Aislamiento Galvánico (Optoacopladores PC817)**: ¿Se colocó optoacoplador por cada canal para separar eléctricamente los bornes exteriores del expansor MCP23017?
- [ ] **Resistencias limitadoras**: ¿Se calcularon las resistencias en serie del LED del optoacoplador para la tensión de entrada esperada (ej. **1k a 1.5k Ohm** para entradas de 12V)?
- [ ] **Diodo antiparalelo**: ¿Tiene cada entrada un diodo (ej. *1N4148*) en antiparalelo con el LED del optoacoplador para protegerlo contra polarizaciones inversas accidentales?
- [ ] **Filtro de Ruido y Pull-ups**: ¿Tiene cada entrada del MCP23017 una resistencia de pull-up a 3.3V (ej. **10k Ohm**) y un capacitor de filtrado a GND (ej. **10nF**) para evitar falsos disparos por hardware?

---

## 4. Salidas de Control
- [ ] **Salidas para SSR Externos (6 Canales)**: 
    - [ ] ¿Las salidas lógicas pasan por un array Darlington **ULN2803/ULN2003** para no cargar de corriente al MCP23017?
    - [ ] ¿Los conectores de salida disponen de un pin con alimentación de **5V** para polarizar los módulos externos y un pin de masa **GND**?
- [ ] **Salidas MOSFET DC en Placa (4 Canales)**:
    - [ ] ¿Se utilizaron MOSFETs Logic-Level (ej. *AOD4184* o *IRLZ44N*) que saturen completamente a 5V/12V?
    - [ ] ¿Se colocó una resistencia de compuerta (*Gate*) de **100 Ohm** en serie?
    - [ ] ¿Tiene cada compuerta una resistencia de pull-down de **10 kΩ** a GND para mantener las salidas inactivas durante el arranque del micro?
    - [ ] ¿Se colocaron diodos Schottky flyback (ej. *SS34*) en paralelo con los bornes de salida para proteger los transistores ante cargas inductivas (sirenas)?

---

## 5. Sensores e Interfaces de Comunicación
- [ ] **Sensores Ultrasonido (JSN-SR04T)**:
    - [ ] ¿Se diseñó la señal de Trigger común hacia los 4 conectores?
    - [ ] ¿Tiene cada una de las 4 líneas de Echo un divisor de tensión resistivo (ej. **1k Ohm** en serie y **2k Ohm** a GND) para bajar la señal del pulso de 5V a 3.3V?
- [ ] **Bus I2C (MCP23017 y pantalla)**: ¿Tiene resistencias de pull-up externas de **4.7 kΩ** en las líneas SDA y SCL a 3.3V?
- [ ] **Bus 1-Wire (Sensores DS18B20)**: ¿Tiene su terminal de conexión con resistencia de pull-up de **4.7 kΩ** (o **2.2 kΩ** para cables largos) conectada a 3.3V?
- [ ] **Protección ESD de Sensores**: ¿Se colocaron arrays de diodos TVS (*USBLC6-2*) en las borneras externas de I2C, 1-Wire y analógicas expuestas al tacto y cableado exterior?
- [ ] **RS485 Aislado**: ¿El transceptor RS485 está alimentado mediante una fuente aislada (ej. *B0505S*) y aislado galvánicamente del microcontrolador?
- [ ] **RS485 Resistencia de Terminación**: ¿Tiene una resistencia de **120 Ohm** en paralelo con las líneas A-B con posibilidad de activarse/desactivarse mediante un jumper físico?
- [ ] **Ethernet (W5500)**: ¿Tiene las resistencias de pull-up recomendadas en las señales del bus SPI y desacoplamiento en su línea de alimentación?

---

## 6. Layout y Reteado
- [ ] **Zona Analógica del ADC**: ¿Las pistas analógicas (GPIOs 1-4) están lo más cortas posibles y alejadas de líneas conmutadas de alta frecuencia?
- [ ] **Retorno de Corrientes (Planos de Masa)**: ¿El plano de masa digital (`GND_DF`) está separado del plano de potencia (`GND_PWR`) y unidos en un único punto estrella de la fuente?
- [ ] **Ancho de pista de alimentación**: ¿Las pistas de 12V y 5V que alimentan relés y MOSFETs tienen un ancho adecuado (mínimo 2mm o polígonos)?
