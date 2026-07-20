# Guía de Conexión USB-C y Protección de Alimentación Dual

Este documento explica cómo implementar el puerto USB-C nativo para el ESP32-S3 en Altium y cómo diseñar el circuito de protección para que la placa pueda alimentarse simultáneamente desde el USB-C (5V) y desde la fuente principal (12V reducidos a 5V por el Buck) sin dañar la computadora ni el regulador.

---

## 1. Conector USB-C: Pines necesarios y conexiones

Para USB 2.0 (como el del ESP32-S3), **no necesitas usar un conector de 24 pines** (que es muy difícil de soldar y rutear). Te recomiendo usar un **conector USB-C de 16 pines** (a veces llamado de 12 pines porque agrupa las masas y alimentaciones).

### Mapeo de Pines del Conector USB-C (16 pines):

```text
  A1/B12 (GND)  A4/B9 (VBUS)  A5 (CC1)   A6 (D+)    A7 (D-)   B5 (CC2)
     [|]           [|]          [|]        [|]        [|]        [|]
      |             |            |          |          |          |
     GND          VBUS         5.1k       22 Ohm     22 Ohm     5.1k
    Placa        (5V USB)        |          |          |          |
                                GND      GPIO 20    GPIO 19      GND
                                         (USB_D+)   (USB_D-)
```

1.  **Masa (GND)**: Conecta todos los pines `GND` (A1, A12, B1, B12) al plano de masa digital (`GND_DF`) de la placa.
2.  **Alimentación (VBUS)**: Conecta todos los pines de alimentación `VBUS` (A4, A9, B4, B9) entre sí. Esta línea transporta los 5V provenientes del puerto USB de la PC.
3.  **Líneas de Datos (D+ y D-)**:
    *   **D+ (A6, B6)**: Conéctalos juntos y llévalos al pin **GPIO 20** (`USB_D+`) del ESP32-S3 a través de una resistencia de **22 Ohm** en serie.
    *   **D- (A7, B7)**: Conéctalos juntos y llévalos al pin **GPIO 19** (`USB_D-`) del ESP32-S3 a través de una resistencia de **22 Ohm** en serie.
    *   *Nota*: Estas resistencias de 22 Ohm sirven para adaptar la impedancia de la línea a 90 Ohm diferenciales.
4.  **Configuración de Puerto (CC1 y CC2)**:
    *   Conecta una resistencia de **5.1 kΩ (precisión 1%)** desde el pin `CC1` (A5) a `GND`.
    *   Conecta otra resistencia de **5.1 kΩ (precisión 1%)** desde el pin `CC2` (B5) a `GND`.
    *   **¡ADVERTENCIA!**: Deben ser **dos resistencias independientes**. No unas los pines CC1 y CC2 para usar una sola resistencia, ya que esto evitaría que ciertos cables modernos de doble conector USB-C (C-to-C) entreguen energía.

---

## 2. Circuitos de Protección de Alimentación Dual

Queremos evitar el "retorno" de corriente (backfeeding). Si conectas el cable USB de la computadora y al mismo tiempo enciendes la fuente de 12V de la placa, el regulador Buck de 5V y el puerto USB de la PC competirán. Esto puede quemar el puerto de la PC o el regulador.

Aquí tienes dos opciones para resolver esto en tu esquemático:

### OPCIÓN 1: La solución simple (Doble Diodo Schottky)
Es la opción más barata y sencilla. Consiste en colocar un diodo en serie con cada fuente de alimentación apuntando hacia la línea interna de `+5V_SYS` de la placa.

```text
VBUS (USB 5V) ------->|-[ Diodo D1: SS14 ]---------+-------> +5V_SYS (Alimentación interna)
                                                   |
5V_BUCK ------------->|-[ Diodo D2: SS14 ]---------+
```

*   **Funcionamiento**: Los diodos actúan como válvulas de retención de una sola vía. Si `5V_BUCK` es mayor que `VBUS`, D2 conduce y D1 se bloquea, impidiendo que la corriente del Buck vaya hacia la computadora. Si el Buck está apagado, D1 conduce y el USB alimenta la placa.
*   **Componente recomendado**: Diodo Schottky **SS14** o **B130** (encapsulado SMA o SOD-123, soportan 1A/30V y tienen muy baja caída de tensión).
*   **Desventaja**: Los diodos Schottky tienen una caída de tensión de unos **0.3V a 0.4V**. Por ende, la línea `+5V_SYS` tendrá realmente unos 4.6V a 4.7V. Esto es suficiente para el regulador LDO de 3.3V (que alimenta el ESP32-S3), pero puede no ser ideal si tienes sensores o relés que requieran exactamente 5.0V.

---

### OPCIÓN 2: La solución profesional (Selector MOSFET automático)
Es la topología que usan las placas oficiales de Arduino. Evita la caída de tensión del diodo en la línea USB usando un MOSFET de canal P como interruptor de estado sólido.

```text
VBUS (USB 5V) -----[ Source  MOSFET Canal-P (AO3401)  Drain ]-----+---> +5V_SYS
                                     |                             |
                                     Gate                          |
                                     |                             |
5V_BUCK -----------------------------+--->|-[ Diodo D2: SS14 ]-----+
                                     |
                                 [ R: 10k ]
                                     |
                                    GND
```

*   **Funcionamiento**:
    *   **Caso 1 (Solo USB conectado)**: `5V_BUCK` está en 0V. La compuerta (Gate) del MOSFET de canal P es llevada a GND (0V) por la resistencia de pull-down de 10k. Esto enciende el MOSFET por completo, dejando pasar los 5V de `VBUS` a `+5V_SYS` con **cero caída de tensión** (gracias a la bajísima resistencia $R_{ds(on)}$ del MOSFET).
    *   **Caso 2 (Alimentación externa de 12V encendida)**: El regulador Buck entrega 5V a la línea `5V_BUCK`. Esto pone la compuerta (Gate) del MOSFET a 5V. Al estar la Gate a la misma tensión que el Source (`VBUS`), el MOSFET **se apaga por completo**, aislando el puerto USB de la computadora. La placa pasa a alimentarse del Buck a través del diodo D2.
*   **Componentes recomendados**:
    *   MOSFET Canal P: **AO3401** (SMD encapsulado SOT-23, muy común y barato).
    *   Diodo D2: **SS14** o **B130**.
