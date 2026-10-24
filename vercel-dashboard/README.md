# 🌐 Sismógrafo IoT - Dashboard Web Remoto (GitHub & Vercel)

Dashboard web con estética **Glassmorphism**, visualización de ondas sísmicas en tiempo real, registro de memoria histórica ("Caja Negra") y monitoreo remoto 24/7 para el Sismógrafo IoT basado en ESP32 y MPU-6050.

---

## 🚀 Despliegue en 3 Minutos: GitHub + Vercel

### Paso 1: Subir a GitHub
1. Abre una terminal (PowerShell o CMD) en esta carpeta (`vercel-dashboard`):
   ```bash
   cd "c:\Users\wilva\Downloads\Sistema Alerta de Sismo\vercel-dashboard"
   ```
2. Inicializa el repositorio y haz tu primer commit:
   ```bash
   git init
   git add .
   git commit -m "feat: Dashboard sismografo iot v2.0 para Vercel"
   ```
3. Crea un nuevo repositorio en [GitHub.com](https://github.com/new) (por ejemplo: `sismografo-iot-dashboard`).
4. Vincula y sube el código:
   ```bash
   git branch -M main
   git remote add origin https://github.com/TU_USUARIO/sismografo-iot-dashboard.git
   git push -u origin main
   ```

---

### Paso 2: Desplegar en Vercel (1 Clic)
1. Ve a [Vercel.com](https://vercel.com/) e inicia sesión con tu cuenta de GitHub.
2. Haz clic en **"Add New..."** -> **"Project"**.
3. Selecciona tu repositorio `sismografo-iot-dashboard` y presiona **"Import"**.
4. Deja la configuración predeterminada (Framework: *Other*, Root Directory: `./`) y haz clic en **"Deploy"**.
5. ¡Listo! Vercel te dará una URL pública gratuita con HTTPS automático:
   `https://tu-sismografo.vercel.app`

---

## 🌍 ¿Cómo monitorear el ESP32 desde fuera de casa?

Dado que Vercel se sirve con cifrado seguro `https://`, los navegadores requieren una conexión segura (`wss://`) para transmitir datos en tiempo real. Tienes dos formas sencillas de conectarte:

### Opción A (Recomendada y 100% Gratuita): Cloudflare Tunnel
No requiere abrir puertos en el router ni configurar IP pública fija.

1. Descarga e instala [Cloudflare Tunnel (`cloudflared`)](https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/downloads/).
2. En tu computadora en casa (en la misma red WiFi del sismógrafo), abre una terminal y ejecuta:
   ```bash
   cloudflared tunnel --url http://192.168.1.36:80
   ```
3. Cloudflare te generará inmediatamente una URL segura pública (ejemplo: `https://xxx-xxx.trycloudflare.com`).
4. Abre tu dashboard en Vercel (`https://tu-sismografo.vercel.app`), haz clic en el ícono de **Nube / Configuración** en la esquina superior derecha, y pega la URL en **"Conexión Remota"**.
5. ¡El dashboard se conectará automáticamente a tu sensor y podrás ver el sismógrafo en vivo desde cualquier teléfono en cualquier parte del mundo!

### Opción B: Modo Simulación / Vista Previa
Si estás de viaje o fuera de casa y tu PC no tiene el túnel encendido, puedes presionar el ícono de **Matraz / Simulador** (<i class="fa-solid fa-flask"></i>) en el encabezado. La interfaz generará ondas y sismos simulados en vivo para inspeccionar el comportamiento y exportar reportes.
