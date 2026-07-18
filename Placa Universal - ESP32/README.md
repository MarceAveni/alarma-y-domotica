# Placa Universal ESP32-S3 (Motherboard / Shield)

Este directorio contiene todo el diseño de hardware, modelos 3D y firmwares unificados bajo la arquitectura de la placa universal basada en el **ESP32-S3**.

## 📂 Estructura de Directorios

*   **`Hardware/`**: Archivos de diseño electrónico.
    *   `Esquematicos/`: Diagramas de circuitos esquemáticos (archivos fuente y PDF de referencia).
    *   `PCB/`: Diseño del circuito impreso (layout y ruteo de pistas).
    *   `Fabricacion/`: Archivos Gerber, lista de componentes (BOM) y coordenadas Centroid/Pick-and-Place para fabricación y ensamble.
*   **`Modelos 3D/`**: Diseños de gabinetes protectores y soportes impresos en 3D específicos para esta placa universal.
*   **`Firmware/`**: Proyectos PlatformIO individuales para cada nodo, compartiendo código común.
    *   `Librerias_Compartidas/`: Librerías y clases C++ comunes para WiFi, MQTT, OTA, gestión de tiempo y programaciones horarias.
    *   `Frente_S3/`: Firmware específico para el nodo del Frente (Sirena, reflectores, PIRs, etc.).
    *   `Patio_S3/`: Firmware específico para el nodo del Patio/Galería (Colectores solares, piscina, nivel de agua, etc.).
    *   `Simple_S3/`: Firmware específico para el nodo Simple (reemplazo del ESP-01 para reflectores auxiliares).

## 🛠️ Compartición de Código (PlatformIO)

Para reutilizar lógica común de red y MQTT sin duplicar código, los proyectos individuales de PlatformIO configuran la directiva `lib_extra_dirs` apuntando a `Librerias_Compartidas`.
