# 🌐 Guía del Sistema Sismógrafo IoT v2.0 (Alta Sensibilidad, Caja Negra y Monitor Vercel)

Documentación completa de las mejoras implementadas en el dispositivo ESP32 con sensor MPU-6050, arquitectura del firmware, visualización sismográfica en tiempo real, registro de memoria histórica y despliegue global en GitHub y Vercel.

---

## 🚀 Resumen de Mejoras Implementadas (Versión 2.0)

### 1. Mayor Sensibilidad y Detección de Micro-Sismos
* **Filtro de Gravedad Optimizado ($\alpha = 0.995$)**: Anteriormente, el filtro pasa-altas adaptativo tenía una constante de $0.98$ (0.5 segundos de tiempo de recuperación), lo que provocaba que movimientos lentos con la mano o sismos pequeños fueran absorbidos y cancelados como si fueran inclinación estática. Con $\alpha = 0.995$, la ventana de absorción se extiende a más de 2.5 segundos, preservando intactas las ondas sísmicas entre 0.5Hz y 10Hz.
* **Pre-inicialización Estática**: Al encender el ESP32, se toma una muestra de 20 lecturas para fijar la gravedad de reposo en $X$, $Y$ y $Z$, eliminando falsas alarmas o derivas iniciales al reiniciar.
* **Rango Extendido y Presets de 1 Clic**:
  * **Ultra-Sensible (0.010 g)**: Detecta pasos tenues en el piso, vibraciones de puertas o micro-sismos casi imperceptibles.
  * **Sensible (0.025 g)**: Configuración recomendada para sismos leves y pruebas manuales con toques suaves.
  * **Normal Hogar (0.050 g)**: Sismos moderados con filtrado de ruidos cotidianos.
  * **Moderado (0.100 g)**: Sismos fuertes (grado V+ en escala Mercalli).
* **Buffer Peak-Hold (100ms)**: Durante cada intervalo de transmisión por WebSockets, el ESP32 retiene el pico máximo absoluto de aceleración, garantizando que sacudidas rápidas no se pierdan entre cuadros de datos.

---

### 2. Visualización Avanzada de Onda (Picos y Valles Claros)
* **Auto-Escala Dinámica Inteligente**: Anteriormente, la escala de dibujo estaba fija en 100x (un movimiento de 0.02g solo se movía 2 píxeles, viéndose casi plano). El nuevo motor gráfico calcula dinámicamente la amplitud de la señal:
  * Para micro-vibraciones (< 0.015g), aplica una **ganancia de 1200x** (12 a 25 píxeles de oscilación oscilante).
  * Para movimientos medios, escala gradualmente (750x, 450x, 240x).
  * En sismos fuertes (> 0.20g), se ajusta a 110x para evitar saturar el lienzo.
* **Selector Manual de Zoom**: Puedes forzar la escala en cualquier momento desde la barra de herramientas del gráfico: `Auto`, `25x`, `10x`, `5x` o `1x`.
* **Trazado Oscilatorio Centrado en Cero**: La onda sismográfica oscila alrededor del eje medio $0.00g$, mostrando picos positivos y valles negativos con iluminación de neón (Cian en reposo, Rojo vibrante durante alarma) y sombreado con gradiente.
* **Lectura Digital de Amplitud Pico a Pico (P2P)**: Muestra en tiempo real la oscilación total en unidades $g$.

---

### 3. Registro de Memoria Histórica ("Caja Negra")
* **Persistencia NVS (No Volátil)**: Cada vez que ocurre un sismo, el ESP32 guarda el evento en su memoria flash interna (NVS `Preferences`). Los datos sobreviven a cortes de luz o reinicios.
* **Datos Registrados por Evento**:
  * **ID Consecutivo**: Número correlativo del movimiento.
  * **Fecha y Hora Exacta**: Sincronizada automáticamente vía NTP con el servidor de hora de Colombia (`co.pool.ntp.org`, UTC-5).
  * **Aceleración Pico (PGA)**: Magnitud máxima en fuerzas $g$.
  * **Intensidad Estimada**: Clasificación automática (Micro-sismo I, Leve II-III, Moderado IV-V, Fuerte VI-VII, Severo VIII+).
  * **Duración**: Tiempo en segundos que duró la sacudida por encima del umbral.
  * **Eje Dominante**: Eje que registró la mayor aceleración ($X$, $Y$ o $Z$).
* **Capacidad**: Buffer circular de hasta 35 eventos con sobreescritura automática de los más antiguos.
* **Descarga en CSV (Excel)**: Botón *"Descargar CSV"* que genera un reporte listo para abrir en Microsoft Excel con todas las columnas formateadas.
* **API REST**: Endpoints nativos `/api/events` (JSON) y `/api/events/clear` (borrado).

---

### 4. Eliminación del Sistema de Pruebas
* **Botón "Probar Alarma" Eliminado**: Se retiró completamente de la interfaz web y de los controles del panel. Ahora el sistema se valida de manera natural moviendo o tocando ligeramente el sensor físico.

---

### 5. Monitoreo Remoto Global: GitHub + Vercel
Para monitorear el sismógrafo desde cualquier celular, tablet o PC en cualquier parte del mundo fuera de tu casa:

#### Paso 1: Subir a GitHub
1. Abre una terminal en la carpeta [`vercel-dashboard`](file:///c:/Users/wilva/Downloads/Sistema%20Alerta%20de%20Sismo/vercel-dashboard):
   ```bash
   cd "c:\Users\wilva\Downloads\Sistema Alerta de Sismo\vercel-dashboard"
   git init
   git add .
   git commit -m "feat: Dashboard Sismografo IoT v2.0"
   ```
2. Crea un repositorio en [GitHub.com](https://github.com/new) llamado `sismografo-iot-dashboard`.
3. Sube el código:
   ```bash
   git branch -M main
   git remote add origin https://github.com/TU_USUARIO/sismografo-iot-dashboard.git
   git push -u origin main
   ```

#### Paso 2: Desplegar en Vercel (1 Clic)
1. Ve a [Vercel.com](https://vercel.com/) e inicia sesión con GitHub.
2. Haz clic en **"Add New..."** -> **"Project"** y selecciona tu repositorio.
3. Presiona **"Deploy"**. Obtendrás tu URL global inmediata con HTTPS: `https://tu-sismografo.vercel.app`.

#### Paso 3: Conexión Remota Segura con Cloudflare Tunnel (100% Gratis)
Debido a que Vercel usa `https://`, los navegadores bloquean conexiones locales sin cifrar. Para conectarte de forma segura desde fuera de tu hogar sin abrir puertos en el router:
1. En tu computadora de casa (en la misma red WiFi del sismógrafo), ejecuta:
   ```bash
   cloudflared tunnel --url http://192.168.1.36:80
   ```
2. Cloudflare te dará una dirección pública segura (ej: `https://sismo-tunel.trycloudflare.com`).
3. Abre tu sitio en Vercel, presiona el botón **Nube / Configuración** (<i class="fa-solid fa-cloud"></i>) arriba a la derecha, pega la dirección y presiona **Conectar Ahora**.
4. ¡El dashboard en Vercel transmitirá en vivo desde tu sensor las 24 horas del día!

*Nota*: Si estás fuera y tu computadora no tiene el túnel encendido, puedes presionar el ícono de **Matraz / Simulador** (<i class="fa-solid fa-flask"></i>) para activar el simulador visual en vivo.

---

### 6. Actualización OTA (Over-The-Air)
No necesitas conectar el ESP32 por cable USB a la computadora para actualizar el firmware:

1. El firmware compilado `.bin` se encuentra en:
   [`seismic_alarm.ino.bin`](file:///c:/Users/wilva/Downloads/Sistema%20Alerta%20de%20Sismo/seismic_alarm/build/seismic_alarm.ino.bin)
2. Abre tu navegador web y entra a:
   `http://192.168.1.36/update`  (o  `http://sismografo.local/update`)
3. Arrastra o selecciona el archivo `seismic_alarm.ino.bin`.
4. Haz clic en **"Subir y Actualizar Firmware"**.
5. La barra de progreso llegará al 100% y el ESP32 se reiniciará automáticamente en 4 segundos con todas las mejoras activas.

---

## 🔌 Conexiones de Hardware (Pinout)

| MPU-6050 | ESP32 GPIO | Función |
| :--- | :--- | :--- |
| **VCC** | **3V3** | Alimentación 3.3V |
| **GND** | **GND** | Tierra física |
| **SCL** | **GPIO 22** | Reloj I2C |
| **SDA** | **GPIO 21** | Datos I2C |
| **Buzzer (+)** | **GPIO 4** | Sirena de audio local |
| **NeoPixel DI** | **GPIO 15** | Control de LEDs WS2812B |
