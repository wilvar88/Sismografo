# 🌍 Sismógrafo IoT v2.0 - Detección Sísmica & Monitoreo Remoto Global

[![Vercel](https://img.shields.io/badge/Deploy-Vercel-black?style=flat&logo=vercel)](https://vercel.com/)
[![ESP32](https://img.shields.io/badge/Hardware-ESP32-blue?style=flat&logo=espressif)](https://www.espressif.com/)
[![Sensor](https://img.shields.io/badge/Sensor-MPU--6050-orange?style=flat)](#conexiones-de-hardware-pinout)
[![OTA](https://img.shields.io/badge/Updates-Over--The--Air-brightgreen?style=flat)](#actualizaciones-ota-inalámbricas)
[![Alexa](https://img.shields.io/badge/Alexa-Sinric%20Pro-purple?style=flat&logo=amazonalexa)](#integración-con-alexa-y-hogar-inteligente)

Sistema integral de detección, registro y monitoreo sísmico en tiempo real con **ESP32** y acelerómetro **MPU-6050**. Cuenta con una interfaz web con estética **Glassmorphism**, visualización dinámica de ondas con auto-escalado hasta 1200x, memoria histórica de eventos ("Caja Negra") con exportación a Excel y despliegue global en la nube mediante **GitHub + Vercel**.

---

## ✨ Características Principales

* 📡 **Alta Sensibilidad Sísmica**: Filtro pasa-altas adaptativo ($\alpha = 0.995$) que preserva ondas sísmicas naturales (0.5Hz a 10Hz) y movimientos tenues con la mano, sin cancelarlos como gravedad estática.
* 🎚️ **Rango Dinámico Extendido (desde 0.005g)** con presets de 1 clic:
  * **Ultra-Sensible (0.010 g)**: Vibraciones de piso, pasos suaves.
  * **Sensible (0.025 g)**: Micro-sismos y pruebas manuales.
  * **Normal Hogar (0.050 g)**: Sismos moderados con filtrado de ruidos cotidianos.
  * **Moderado (0.100 g)**: Sismos de alta intensidad (Mercalli V+).
* 📈 **Visualización Avanzada de Onda (Picos y Valles Claros)**:
  * Auto-escalado inteligente (hasta 1200x) y selector manual de zoom (`Auto`, `25x`, `10x`, `5x`, `1x`).
  * Trazado de oscilación centrado en $0.00g$ con iluminación neón cian/rojo y sombreado con gradiente.
  * Lectura digital instantánea de amplitud Pico a Pico (P2P g).
* 💾 **Memoria Histórica ("Caja Negra")**:
  * Registro persistente en memoria NVS (`Preferences`) de hasta 35 eventos sísmicos que no se borran al reiniciar.
  * Sincronización horaria vía NTP oficial para Colombia (`co.pool.ntp.org`, UTC-5).
  * Guarda: Timestamp, aceleración máxima (PGA), grado estimado en Escala Mercalli, duración en segundos y eje dominante.
  * Botón para **Descargar Reporte CSV** compatible directamente con Microsoft Excel.
* ☁️ **Monitoreo Global Autónomo (Cloud MQTT 24/7 - Cero PC)**: El ESP32 transmite su telemetría directamente a un broker MQTT seguro en la nube (`broker.emqx.io`). Tu panel en **Vercel** se conecta mediante `wss://` desde cualquier lugar del mundo **sin necesidad de dejar ninguna computadora encendida en tu casa**.
* 🌐 **Conexión Híbrida Inteligente**: Si estás en casa en tu WiFi, el panel se conecta directo al ESP32 por WebSockets de ultra baja latencia. Si estás en la calle con datos móviles, cambia automáticamente a Cloud MQTT 24/7.
* 🧪 **Modo Simulador Integrado**: Permite previsualizar la interfaz y probar alertas con sismos simulados desde el celular sin necesidad de tener el sensor conectado.
* 🚀 **Actualizaciones Inalámbricas OTA**: Página web `/update` integrada para cargar nuevos firmwares `.bin` vía WiFi sin desconectar el equipo de la pared.
* 🗣️ **Notificaciones Push y Alexa**: Sensor de movimiento virtual mediante **Sinric Pro** para detonar rutinas de voz inmediatas en altavoces Echo y enviar notificaciones emergentes a tu celular en cualquier lugar del mundo.

---

## 🚀 Despliegue en Vercel (1 Clic)

Este repositorio está preparado para desplegarse inmediatamente en Vercel:

1. Ve a [Vercel.com](https://vercel.com/) e inicia sesión con tu cuenta de GitHub.
2. Haz clic en **"Add New..."** $\rightarrow$ **"Project"**.
3. Selecciona el repositorio **`Sismografo`** y presiona **"Import"**.
4. Deja la configuración predeterminada:
   * **Framework Preset:** *Other*
   * **Root Directory:** `./`
5. Haz clic en **"Deploy"**.
6. ¡Listo! Vercel te entregará una URL global con HTTPS automático:
   `https://tu-sismografo.vercel.app`

---

## ☁️ Monitoreo Global Autónomo (Cloud MQTT 24/7)

¡Tu sismógrafo es **100% independiente**! No necesitas tener ninguna PC encendida en tu hogar:

```
[ ESP32 en tu pared ] ---> WiFi Hogar ---> [ Broker Cloud EMQX ] ---> [ Tu Celular en Vercel ]
```

1. El ESP32 publica los datos dinámicos a `broker.emqx.io` por el puerto 1883.
2. Al abrir tu enlace de Vercel en cualquier navegador o celular, se suscribe automáticamente vía WebSockets Seguros (`wss://broker.emqx.io:8084/mqtt`).
3. Puedes ver la onda en vivo, cambiar la sensibilidad y recalibrar el sensor desde cualquier país del mundo.

---

## 🔔 Notificaciones Push en tu Celular (Alexa & Sinric Pro)

Para recibir alertas flotantes en la pantalla de bloqueo de tu celular cuando ocurra un movimiento:

1. Abre la app **Amazon Alexa** en tu teléfono $\rightarrow$ **Más** $\rightarrow$ **Rutinas**.
2. Selecciona la rutina de tu sismógrafo $\rightarrow$ Toca el botón **`+`** (Agregar acción).
3. Selecciona **Notificaciones** $\rightarrow$ **Notificación**.
4. Escribe el mensaje deseado (ej: `🚨 ¡ALERTA SÍSMICA! Movimiento detectado en Torre 7 Apto 902`).
5. Guarda la rutina. Cada vez que el sensor MPU-6050 se active, tu celular vibrará y te notificará al instante.

## 🔌 Conexiones de Hardware (Pinout)

| Sensor / Periférico | Pin ESP32 | Descripción |
| :--- | :--- | :--- |
| **MPU-6050 VCC** | **3V3** | Alimentación 3.3V |
| **MPU-6050 GND** | **GND** | Tierra común |
| **MPU-6050 SCL** | **GPIO 22** | Reloj I2C (SCL) |
| **MPU-6050 SDA** | **GPIO 21** | Línea de datos I2C (SDA) |
| **Buzzer (+)** | **GPIO 4** | Alarma sonora / zumbador |
| **LEDs WS2812B (DI)**| **GPIO 15** | Tira o anillo NeoPixel |

---

## 🛠️ Estructura del Repositorio

```
├── index.html            # Dashboard web principal Glassmorphism (Vercel)
├── vercel.json           # Configuración de enrutamiento y caché para Vercel
├── package.json          # Manifiesto de la aplicación web
├── .gitignore            # Archivos y binarios intermedios ignorados
├── README.md             # Documentación general del proyecto
├── walkthrough.md        # Guía técnica paso a paso y solución de problemas
└── seismic_alarm/        # Código fuente del firmware Arduino ESP32
    ├── seismic_alarm.ino # Firmware C++ principal (100Hz MPU6050, NVS, WebSockets, Sinric Pro)
    ├── web_dashboard.h   # Dashboard comprimido en memoria flash del ESP32
    └── sismografo.html   # Plantilla HTML de desarrollo
```

---

## 🔄 Actualizaciones OTA (Inalámbricas)

Una vez grabado el firmware con el esquema de partición **`Minimal SPIFFS (1.9MB APP with OTA)`**, cualquier actualización futura se realiza por WiFi:

1. Entra a `http://192.168.1.36/update` en tu navegador.
2. Selecciona el nuevo archivo compilado `.bin`.
3. Haz clic en **Subir y Actualizar Firmware**.
4. El dispositivo se reinicia automáticamente en 4 segundos sin desconectarlo de la pared.

---

## 📄 Licencia

Este proyecto es de código abierto bajo la licencia [MIT](LICENSE).
