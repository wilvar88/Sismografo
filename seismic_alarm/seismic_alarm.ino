/**
 * Sistema de Alerta Sísmica IoT con ESP32 y MPU-6050
 * 
 * Este programa realiza las siguientes tareas:
 * 1. Conexión WiFi con redundancia (Intenta conectarse a "Varela" y "Familia Varela Paez 2.4").
 * 2. Comunicación I2C con MPU-6050 para leer aceleración tridimensional.
 * 3. Algoritmo de eliminación de gravedad estática (Filtro Pasa-Altas) para detectar 
 *    vibraciones sísmicas independientes de la orientación o inclinación física del sensor.
 * 4. Servidor Web local en puerto 80 para servir la interfaz gráfica Glassmorphism.
 * 5. Servidor WebSocket en puerto 81 para transmitir telemetría sísmica en tiempo real.
 * 6. Integración Alexa Local (Espalexa): Dispositivo virtual tipo Switch/Luz.
 * 7. Integración Tuya/Smart Life mediante disparador de Webhook de IFTTT.
 * 8. Soporte para alertas locales con tira de LEDs NeoPixel (WS2812B) y Buzzer.
 * 9. OTA (Over-The-Air): Actualización de firmware por WiFi via Arduino IDE o navegador web.
 *    - Arduino IDE: Aparece en Herramientas > Puerto > Red > sismografo
 *    - Navegador:   http://sismografo.local/update  (o http://<IP>/update)
 *    - Contraseña OTA: sismo2024
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>          // mDNS: acceso via sismografo.local
#include <Update.h>           // OTA via navegador web (/update)
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Preferences.h>

// --- LIBRERÍAS DE SOPORTE CONDICIONAL / INTEGRACIÓN ---
#include <Adafruit_NeoPixel.h>
#include <PubSubClient.h>
#include <SinricPro.h>
#include <SinricProMotionsensor.h>
#include "web_dashboard.h"

// --- NOMBRE MDNS Y OTA ---
#define MDNS_HOSTNAME  "sismografo"   // Acceder via: http://sismografo.local
#define OTA_PASSWORD   "sismo2024"    // Contraseña para subidas OTA (cambia esto!)

// --- CONFIGURACIÓN DE HARDWARE ---
#define NEOPIXEL_PIN     15   // Pin de salida para tira WS2812B (conectar DI de tira aquí)
#define NUM_LEDS         8    // Cantidad de LEDs en tu tira o anillo
#define BUZZER_PIN       4    // Pin para zumbador (Buzzer pasivo o activo)
#define I2C_SDA          21   // Pin predeterminado SDA del ESP32
#define I2C_SCL          22   // Pin predeterminado SCL del ESP32

// --- CREDENCIALES WIFI ---
const int NUM_NETWORKS = 2;
const char* SSIDS[NUM_NETWORKS] = { "Varela", "Familia Varela Paez 2.4" };
const char* PASSWORDS[NUM_NETWORKS] = { "Holacomoestas14", "Holacomoestas14" };

// --- INSTANCIAS DE OBJETOS ---
Adafruit_MPU6050 mpu;
WebServer server(80);
WebSocketsServer webSocket(81);
Preferences preferences;
Adafruit_NeoPixel strip(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// --- CLIENTE Y CONFIGURACIÓN MQTT CLOUD (Monitoreo Global 24/7 sin PC) ---
WiFiClient espMqttClient;
PubSubClient mqttClient(espMqttClient);
const char* MQTT_BROKER       = "broker.emqx.io";
const int   MQTT_PORT         = 1883;
const char* MQTT_TOPIC_DATA   = "sismografo/wilvar88/data";
const char* MQTT_TOPIC_EVENTS = "sismografo/wilvar88/events";
const char* MQTT_TOPIC_CMD    = "sismografo/wilvar88/cmd";
const char* MQTT_TOPIC_STATUS = "sismografo/wilvar88/status";
unsigned long lastMqttReconnect = 0;

// --- ESTRUCTURA Y MEMORIA HISTÓRICA DE EVENTOS SÍSMICOS ("CAJA NEGRA") ---
struct SeismicEvent {
  uint32_t id;
  char time[24];
  float pga;
  float duration;
  char axis;
  char intensity[22];
};

#define MAX_EVENTS 35
SeismicEvent eventsLog[MAX_EVENTS];
int eventCount = 0;
int nextEventIndex = 0;

// Variables de seguimiento del evento en curso
float currentEventMaxMag = 0.0;
unsigned long currentEventStartTime = 0;
char currentEventAxis = 'Z';
float windowMaxMag = 0.0; // Retiene el pico máximo en la ventana de telemetría (100ms)

// --- VARIABLES DE CONFIGURACIÓN Y CONTROL ---
float threshold = 0.025;      // Sensibilidad por defecto en 0.025g (sismos leves / alta sensibilidad)
int alarmDuration = 10;       // Duración de la alarma activa en segundos

// Offsets de calibración (obtenidos dinámicamente)
float calOffsetX = 0.0;
float calOffsetY = 0.0;
float calOffsetZ = 0.0;

// Variables de estado del IFTTT Webhook
String iftttKey = "";
String iftttEvent = "sismo_alert";


// Variables de configuración de Red WiFi Dinámica
String wifiSsid = "";
String wifiPass = "";

// Variables de configuración de Sinric Pro (Alexa Virtual Sensor)
String sinricAppKey = "";
String sinricAppSecret = "";
String sinricDeviceId = "";
bool sinricEnabled = false;

// Variables del Filtro Pasa-Altas (Baseline tracking para eliminar gravedad estática)
float xSlow = 0.0;
float ySlow = 0.0;
float zSlow = 1.0; // En unidades G

// Variables del ciclo de vida de la alarma
bool alarmActive = false;
unsigned long alarmStartTime = 0;
unsigned long lastTelemetryTime = 0;
unsigned long lastBuzzerToggleTime = 0;
bool buzzerState = false;

// Prototipos de funciones
void initWiFi();
void initMDNS();
void handleRoot();
void handleAPIConfig();
void handleAPICalibrate();
void handleAPIGetEvents();
void handleAPIClearEvents();
void handleAPIIfttt();
void handleAPIInfo();
void handleOTAPage();
void handleOTAUpload();
void triggerAlarm();
void deactivateAlarm();
void updateAlarmEffects();
void broadcastTelemetry();
String getFormattedUptime();
String getFormattedDateTime();
void recordSeismicEvent(float maxMag, float durSec, char domAxis);
void loadEventsFromNVS();
void saveEventsToNVS();
void triggerDirectTuyaAlert();
String sha256Hex(String payload);
String hmacSha256Hex(String key, String message);
String getTimestampMS();
bool isTimeSynced();
void handleAPIGetConfig();
void handleAPIWifi();
void handleAPISinricConfig();
void checkMqtt();
void mqttCallback(char* topic, byte* payload, unsigned int length);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== SISTEMA DE ALERTA DE SISMO IOT - INICIANDO ===");

  // 1. Inicialización de Hardware
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  strip.begin();
  strip.show(); // Apagar todos los LEDs al inicio
  strip.setBrightness(150); // Brillo moderado

  // 2. Cargar configuraciones guardadas de NVS Preferences
  preferences.begin("seismic-alarm", false);
  threshold = preferences.getFloat("threshold", 0.025);
  alarmDuration = preferences.getInt("duration", 10);
  calOffsetX = preferences.getFloat("calOffsetX", 0.0);
  calOffsetY = preferences.getFloat("calOffsetY", 0.0);
  calOffsetZ = preferences.getFloat("calOffsetZ", 0.0);
  iftttKey = preferences.getString("iftttKey", "");
  iftttEvent = preferences.getString("iftttEvent", "sismo_alert");
  wifiSsid = preferences.getString("wifiSsid", "");
  wifiPass = preferences.getString("wifiPass", "");
  sinricAppKey = preferences.getString("sinricAppKey", "");
  sinricAppSecret = preferences.getString("sinricAppSecret", "");
  sinricDeviceId = preferences.getString("sinricDeviceId", "");
  preferences.end();
  
  // Cargar registros históricos de sismos guardados en memoria NVS
  loadEventsFromNVS();

  Serial.printf("[CFG] Thr:%.3fg Dur:%ds\n", threshold, alarmDuration);
  
  Serial.print("IFTTT Configurado: ");
  Serial.println(iftttKey.length() > 0 ? "SÍ (Key cargada)" : "NO");

  // 3. Inicialización del Sensor MPU-6050
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!mpu.begin()) {
    Serial.println("[ERR] MPU6050 no detectado");
    // Parpadear LEDs en rojo indicando falla crítica
    for (int i=0; i<10; i++) {
      strip.fill(strip.Color(255, 0, 0)); strip.show(); delay(200);
      strip.fill(strip.Color(0, 0, 0)); strip.show(); delay(200);
    }
  } else {
    Serial.println("[OK] MPU6050 listo");
    // Configurar rangos del sensor
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G); // Rango de +/- 2G para máxima sensibilidad sísmica
    mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ); // Filtro pasa-bajos en sensor para quitar ruidos de alta frecuencia
    
    // Pre-cargar la gravedad estática inicial para evitar desvío transitorio al arrancar
    sensors_event_t aInit, gInit, tInit;
    mpu.getEvent(&aInit, &gInit, &tInit);
    xSlow = aInit.acceleration.x / 9.80665 - calOffsetX;
    ySlow = aInit.acceleration.y / 9.80665 - calOffsetY;
    zSlow = aInit.acceleration.z / 9.80665 - calOffsetZ;

    // LEDs azul/cian de confirmación
    strip.fill(strip.Color(0, 255, 255)); strip.show(); delay(800);
    strip.fill(strip.Color(0, 0, 0)); strip.show();
  }

  // 4. Conexión WiFi
  initWiFi();
  
  // Configurar NTP para sincronización de hora local precisa (Colombia UTC-5)
  configTime(-18000, 0, "co.pool.ntp.org", "pool.ntp.org");

  // 5. Configuración del Servidor Web REST API
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/config", HTTP_GET, handleAPIConfig);
  server.on("/api/calibrate", HTTP_GET, handleAPICalibrate);
  server.on("/api/events", HTTP_GET, handleAPIGetEvents);
  server.on("/api/events/clear", HTTP_GET, handleAPIClearEvents);
  server.on("/api/ifttt", HTTP_GET, handleAPIIfttt);
  server.on("/api/tuyaconfig", HTTP_GET, [](){ server.send(200, "application/json", "{\"status\":\"ok\"}"); });
  server.on("/api/get_config", HTTP_GET, handleAPIGetConfig);
  server.on("/api/wifi", HTTP_GET, handleAPIWifi);
  server.on("/api/sinricconfig", HTTP_GET, handleAPISinricConfig);
  server.on("/api/info", HTTP_GET, handleAPIInfo);
  
  // Ruta OTA Web: GET muestra la página, POST recibe el .bin
  server.on("/update", HTTP_GET, handleOTAPage);
  server.on("/update", HTTP_POST,
    []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", Update.hasError() ? "FALLO: Error en la actualizacion" : "OK: Firmware actualizado. Reiniciando...");
      delay(500);
      ESP.restart();
    },
    handleOTAUpload
  );
  
  // Manejador para cuando una ruta no se encuentra
  server.onNotFound([]() {
    server.send(404, "application/json", "{\"error\":\"Not Found\"}");
  });

  // 6. Servidor WebSocket
  webSocket.begin();
  webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    // Manejar eventos de conexión si es necesario
    if (type == WStype_CONNECTED) {
      Serial.printf("Cliente WebSocket conectado ID: %d\n", num);
    }
  });

  // 8. Inicialización de Sinric Pro (Sensor de Sismo Virtual)
  if (sinricAppKey.length() > 0 && sinricAppSecret.length() > 0 && sinricDeviceId.length() > 0) {
    sinricEnabled = true;
    // Registrar sensor de movimiento
    SinricProMotionsensor &mySensor = SinricPro[sinricDeviceId.c_str()];
    SinricPro.begin(sinricAppKey.c_str(), sinricAppSecret.c_str());
    Serial.println("[OK] SinricPro");
  } else {
    
  }

  // 9. Iniciar mDNS para que el dispositivo sea accesible como sismografo.local
  initMDNS();

  
  // 10. Configurar cliente MQTT Cloud (EMQX)
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);

  server.begin();
  Serial.println("[OK] Sistema listo");
  Serial.printf("Dashboard : http://%s.local  o  http://%s\n", MDNS_HOSTNAME, WiFi.localIP().toString().c_str());
  Serial.printf("OTA Web   : http://%s.local/update\n", MDNS_HOSTNAME);
}

void loop() {
  // Correr bucles de servidores de fondo
  server.handleClient();
  webSocket.loop();
  checkMqtt();
  if (sinricEnabled) {
    SinricPro.handle();
  }

  // --- 1. LECTURA Y PROCESAMIENTO DEL ACELERÓMETRO ---
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Convertir lecturas crudas en unidades G (1G ≈ 9.80665 m/s2)
  float gx = a.acceleration.x / 9.80665 - calOffsetX;
  float gy = a.acceleration.y / 9.80665 - calOffsetY;
  float gz = a.acceleration.z / 9.80665 - calOffsetZ;

  // Filtro Pasa-Altas dinámico (Baseline tracking)
  // Coeficiente 0.995: seguimiento ultra lento de la gravedad para que los sismos de 0.5Hz a 10Hz
  // y movimientos simulados por la mano no sean cancelados por la gravedad estática.
  float filterCoefficient = 0.995;
  xSlow = (xSlow * filterCoefficient) + (gx * (1.0 - filterCoefficient));
  ySlow = (ySlow * filterCoefficient) + (gy * (1.0 - filterCoefficient));
  zSlow = (zSlow * filterCoefficient) + (gz * (1.0 - filterCoefficient));

  // Restar la gravedad (baseline) para obtener los vectores dinámicos puros (oscilando alrededor de 0G)
  float dx = gx - xSlow;
  float dy = gy - ySlow;
  float dz = gz - zSlow;

  // Calcular la magnitud del vector de vibración dinámico absoluto (en g-force)
  float dynamicVibeMag = sqrt(dx*dx + dy*dy + dz*dz);

  // Retención del pico máximo en la ventana de telemetría de 100ms
  if (dynamicVibeMag > windowMaxMag) {
    windowMaxMag = dynamicVibeMag;
  }

  // --- 2. EVALUACIÓN DEL UMBRAL DE SISMO ---
  if (dynamicVibeMag > threshold) {
    if (!alarmActive) {
      Serial.printf("¡ALERTA! Sismo detectado. Vibración: %.4fg, Umbral: %.3fg\n", dynamicVibeMag, threshold);
      triggerAlarm();
      currentEventMaxMag = dynamicVibeMag;
      currentEventStartTime = millis();
      
      // Determinar eje de mayor impacto
      float ax = fabs(dx), ay = fabs(dy), az = fabs(dz);
      if (ax >= ay && ax >= az) currentEventAxis = 'X';
      else if (ay >= ax && ay >= az) currentEventAxis = 'Y';
      else currentEventAxis = 'Z';
    } else {
      // Si la alarma ya está activa, actualizar el pico máximo si el sismo se intensifica
      if (dynamicVibeMag > currentEventMaxMag) {
        currentEventMaxMag = dynamicVibeMag;
        float ax = fabs(dx), ay = fabs(dy), az = fabs(dz);
        if (ax >= ay && ax >= az) currentEventAxis = 'X';
        else if (ay >= ax && ay >= az) currentEventAxis = 'Y';
        else currentEventAxis = 'Z';
      }
    }
  }

  // --- 3. CICLO DE VIDA DE LA ALERTA ACTIVA Y REGISTRO EN MEMORIA ---
  if (alarmActive) {
    if (millis() - alarmStartTime > (alarmDuration * 1000)) {
      if (dynamicVibeMag < (threshold * 0.75)) {
        float dur = (millis() - currentEventStartTime) / 1000.0f;
        // Guardar hallazgo de memoria permanente con hora, fecha, aceleración pico y grados
        recordSeismicEvent(currentEventMaxMag, dur, currentEventAxis);
        Serial.println("Sismo finalizado o estabilizado. Evento guardado en historial. Desactivando alarmas.");
        deactivateAlarm();
      }
    }
  }

  // --- 4. EFECTOS LOCALES DE ALERTA (LEDs y BUZZER) ---
  updateAlarmEffects();

  // --- 5. TRANSMISIÓN DE TELEMETRÍA (10Hz para sismógrafo web fluido) ---
  if (millis() - lastTelemetryTime > 100) {
    lastTelemetryTime = millis();
    
    // Usamos el pico de la ventana de 100ms para garantizar que ningún pico se pierda entre frames
    float broadcastMag = windowMaxMag;
    windowMaxMag = 0.0;
    
    // Formato: dx, dy, dz, broadcastMag, threshold, alarmActive, uptime, eventCount
    String telemetryData = String(dx, 4) + "," + 
                          String(dy, 4) + "," + 
                          String(dz, 4) + "," + 
                          String(broadcastMag, 4) + "," + 
                          String(threshold, 3) + "," + 
                          String(alarmActive ? 1 : 0) + "," + 
                          getFormattedUptime() + "," +
                          String(eventCount);
                          
    webSocket.broadcastTXT(telemetryData);
    if (mqttClient.connected()) {
      mqttClient.publish(MQTT_TOPIC_DATA, telemetryData.c_str());
    }
  }

  delay(10); // Muestreo estable a ~100Hz
}

// --- MDNS: Registro de Hostname en la Red Local ---
void initMDNS() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[mDNS] No hay WiFi activo, omitiendo mDNS.");
    return;
  }
  if (MDNS.begin(MDNS_HOSTNAME)) {
    // Anunciar el servicio HTTP para que la página de descubrimiento lo encuentre
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws",   "tcp", 81);
    Serial.printf("[mDNS] Dispositivo registrado como: http://%s.local\n", MDNS_HOSTNAME);
  } else {
    Serial.println("[mDNS] Error al iniciar mDNS. La IP directa seguirá funcionando.");
  }
}

// --- CONEXIÓN WIFI CON REDUNDANCIA ---
void initWiFi() {
  WiFi.mode(WIFI_STA);
  
  // LEDs en cian parpadeante para indicar búsqueda de red
  strip.fill(strip.Color(0, 150, 150)); strip.show();

  bool connected = false;

  // 1. Intentar conectar a la red WiFi dinámica configurada por el usuario si existe
  if (wifiSsid.length() > 0) {
    Serial.printf("Intentando conectar a WiFi configurado por usuario SSID: %s... \n", wifiSsid.c_str());
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
    
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(500);
      Serial.print(".");
      retries++;
      
      // Alternar LEDs de carga
      if (retries % 2 == 0) {
        strip.fill(strip.Color(0, 100, 100)); strip.show();
      } else {
        strip.fill(strip.Color(0, 0, 0)); strip.show();
      }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      Serial.println("\n¡Conectado exitosamente a la red del usuario!");
    } else {
      Serial.println("\nFallo conexión a red del usuario. Intentando redes de respaldo...");
    }
  }

  // 2. Si no está conectado, barrer las redes de respaldo cableadas en el sketch
  if (!connected) {
    for (int j = 0; j < 3; j++) {
      for (int i = 0; i < NUM_NETWORKS; i++) {
        Serial.printf("Intentando conectar a WiFi de respaldo SSID: %s... \n", SSIDS[i]);
        WiFi.begin(SSIDS[i], PASSWORDS[i]);
        
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 20) {
          delay(500);
          Serial.print(".");
          retries++;
          
          if (retries % 2 == 0) {
            strip.fill(strip.Color(0, 100, 100)); strip.show();
          } else {
            strip.fill(strip.Color(0, 0, 0)); strip.show();
          }
        }
        
        if (WiFi.status() == WL_CONNECTED) {
          connected = true;
          Serial.println("\n¡Conectado exitosamente!");
          break;
        }
        Serial.println("\nFallo en la conexión.");
      }
      if (connected) break;
    }
  }

  // Mostrar resultado de conexión
  if (connected) {
    Serial.print("Dirección IP local del ESP32: ");
    Serial.println(WiFi.localIP());
    
    // LEDs en verde sólido por 2 segundos indicando conexión exitosa
    strip.fill(strip.Color(0, 255, 0)); strip.show();
    delay(2000);
    strip.fill(strip.Color(0, 0, 0)); strip.show();
  } else {
    // 3. AP de Emergencia si todo falla
    Serial.println("No se pudo conectar a ninguna red WiFi provista.");
    Serial.println("Iniciando Punto de Acceso (AP) de emergencia: ESP32-AlertaSismo (Clave: 12345678)");
    WiFi.softAP("ESP32-AlertaSismo", "12345678");
    Serial.print("IP del Punto de Acceso: ");
    Serial.println(WiFi.softAPIP());
    
    // LEDs en naranja sólido para denotar modo AP
    strip.fill(strip.Color(255, 100, 0)); strip.show();
  }
}

// --- DISPARADORES DE ALERTA ---
void triggerAlarm() {
  alarmActive = true;
  alarmStartTime = millis();

  // 1. Disparar webhook a IFTTT si está configurado (usando WiFiClient ligero)
  if (iftttKey.length() > 0) {
    Serial.println("Enviando Webhook de alerta sísmica a IFTTT...");
    xTaskCreate([](void* parameter) {
      WiFiClient client;
      if (client.connect("maker.ifttt.com", 80)) {
        client.print(String("GET /trigger/") + iftttEvent + "/with/key/" + iftttKey + " HTTP/1.1\r\nHost: maker.ifttt.com\r\nConnection: close\r\n\r\n");
        client.stop();
        Serial.println("[IFTTT] Webhook enviado con éxito.");
      }
      vTaskDelete(NULL);
    }, "iftttTask", 2048, NULL, 1, NULL);
  }
  
  // 2. Disparar evento de movimiento a Sinric Pro (Alexa Virtual Sensor para anuncios de voz y enchufes)
  if (sinricEnabled) {
    SinricProMotionsensor &mySensor = SinricPro[sinricDeviceId.c_str()];
    mySensor.sendMotionEvent(true); // Movimiento detectado
    Serial.println("[Sinric Pro] Alerta de sismo enviada (Movimiento Detectado a Alexa).");
  }
}

void deactivateAlarm() {
  alarmActive = false;
  
  // Apagar el Buzzer y limpiar Neopixels
  digitalWrite(BUZZER_PIN, LOW);
  strip.fill(strip.Color(0, 0, 0));
  strip.show();

  // Restablecer estado de Sinric Pro
  if (sinricEnabled) {
    SinricProMotionsensor &mySensor = SinricPro[sinricDeviceId.c_str()];
    mySensor.sendMotionEvent(false); // Movimiento terminado
    Serial.println("[Sinric Pro] Alerta de sismo terminada (Movimiento Despejado).");
  }
}

// --- EFECTOS LOCALES DE ALERTA (LEDs y BUZZER) ---
void updateAlarmEffects() {
  if (alarmActive) {
    // 1. Generar pulso de sonido en el Buzzer (Alternar cada 150ms para un pitido rápido)
    if (millis() - lastBuzzerToggleTime > 150) {
      lastBuzzerToggleTime = millis();
      buzzerState = !buzzerState;
      digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
    }

    // 2. Efecto de Luces Estroboscópicas Rojas en los NeoPixels
    // Alternamos entre rojo brillante en todos los LEDs y apagado/tenue rápido
    int flashPeriod = (millis() / 150) % 2;
    if (flashPeriod == 0) {
      strip.fill(strip.Color(255, 0, 0)); // Rojo alarma
    } else {
      strip.fill(strip.Color(0, 0, 0));   // Apagado
    }
    strip.show();
  } else {
    // Estado de reposo: LEDs apagados o puedes poner un brillo verde respirable tenue
    // Para no perturbar de noche, los dejamos apagados. El buzzer se asegura apagado.
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// --- MANEJADORES DE SERVIDOR WEB (PÁGINA E INTERFAZ API) ---

void handleRoot() {
  // Servir dashboard HTML comprimido en gzip nativo para máxima velocidad y ahorro de flash
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html", (const char*)HTML_INDEX, HTML_INDEX_LEN);
}

// Endpoint de descubrimiento / info del dispositivo
void handleAPIInfo() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String mac = WiFi.macAddress();
  String ip  = WiFi.localIP().toString();
  String json = "{";
  json += "\"device\":\"Sismografo IoT\",";
  json += "\"hostname\":\"" + String(MDNS_HOSTNAME) + ".local\",";
  json += "\"ip\":\"" + ip + "\",";
  json += "\"mac\":\"" + mac + "\",";
  json += "\"ws_port\":81,";
  json += "\"uptime\":\"" + getFormattedUptime() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleAPIConfig() {
  // Endpoint: /api/config?threshold=0.25&duration=15
  if (server.hasArg("threshold")) {
    threshold = server.arg("threshold").toFloat();
  }
  if (server.hasArg("duration")) {
    alarmDuration = server.arg("duration").toInt();
  }

  // Guardar en la NVS del ESP32
  preferences.begin("seismic-alarm", false);
  preferences.putFloat("threshold", threshold);
  preferences.putInt("duration", alarmDuration);
  preferences.end();

  Serial.printf("Nuevos ajustes aplicados y guardados: Umbral=%.2fg, Duración=%ds\n", threshold, alarmDuration);
  server.send(200, "application/json", "{\"status\":\"success\",\"threshold\":" + String(threshold) + ",\"duration\":" + String(alarmDuration) + "}");
}

void handleAPICalibrate() {
  Serial.println("Iniciando recalibración de offsets del acelerómetro...");
  
  float sumX = 0;
  float sumY = 0;
  float sumZ = 0;
  const int samples = 100;

  // Encender LEDs en amarillo estático durante la calibración
  strip.fill(strip.Color(255, 150, 0)); strip.show();

  // Tomamos 100 lecturas promediadas del sensor en reposo estático
  for (int i = 0; i < samples; i++) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    
    // Sumamos la aceleración cruda convertida a G
    sumX += a.acceleration.x / 9.80665;
    sumY += a.acceleration.y / 9.80665;
    // Para Z, restamos 1.0g (gravedad base) ya que esperamos que esté plano y mida 1G
    sumZ += (a.acceleration.z / 9.80665) - 1.0;
    
    delay(20);
  }

  // Asignamos los nuevos offsets
  calOffsetX = sumX / samples;
  calOffsetY = sumY / samples;
  calOffsetZ = sumZ / samples;

  // Guardar offsets en Preferences
  preferences.begin("seismic-alarm", false);
  preferences.putFloat("calOffsetX", calOffsetX);
  preferences.putFloat("calOffsetY", calOffsetY);
  preferences.putFloat("calOffsetZ", calOffsetZ);
  preferences.end();

  Serial.printf("Calibración finalizada. Nuevos offsets -> X: %.4f, Y: %.4f, Z: %.4f\n", calOffsetX, calOffsetY, calOffsetZ);
  
  // Parpadear verde para avisar fin de calibración
  for (int i=0; i<3; i++) {
    strip.fill(strip.Color(0, 255, 0)); strip.show(); delay(200);
    strip.fill(strip.Color(0, 0, 0)); strip.show(); delay(200);
  }

  if (server.client()) {
    server.send(200, "application/json", "{\"status\":\"success\",\"offset_x\":" + String(calOffsetX, 4) + ",\"offset_y\":" + String(calOffsetY, 4) + "}");
  }
}

// --- GESTIÓN DE MEMORIA HISTÓRICA DE SISMOS ("CAJA NEGRA") ---

String getFormattedDateTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char buf[30];
  if (timeinfo->tm_year > 120) {
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", timeinfo);
    return String(buf);
  }
  sprintf(buf, "Uptime %s", getFormattedUptime().c_str());
  return String(buf);
}

void loadEventsFromNVS() {
  preferences.begin("seismic-events", true);
  eventCount = preferences.getInt("count", 0);
  nextEventIndex = preferences.getInt("next", 0);
  if (eventCount > MAX_EVENTS) eventCount = MAX_EVENTS;
  if (eventCount > 0) {
    preferences.getBytes("log", eventsLog, sizeof(eventsLog));
  }
  preferences.end();
  Serial.printf("[HISTORIAL] %d evts\n", eventCount);
}

void saveEventsToNVS() {
  preferences.begin("seismic-events", false);
  preferences.putInt("count", eventCount);
  preferences.putInt("next", nextEventIndex);
  preferences.putBytes("log", eventsLog, sizeof(eventsLog));
  preferences.end();
}

void recordSeismicEvent(float maxMag, float durSec, char domAxis) {
  if (maxMag < 0.005) return; // Filtrar ruidos insignificantes
  
  SeismicEvent ev;
  ev.id = (eventCount > 0) ? (eventsLog[(nextEventIndex - 1 + MAX_EVENTS) % MAX_EVENTS].id + 1) : 1;
  String dt = getFormattedDateTime();
  strncpy(ev.time, dt.c_str(), sizeof(ev.time) - 1);
  ev.time[sizeof(ev.time) - 1] = '\0';
  ev.pga = maxMag;
  ev.duration = durSec;
  ev.axis = domAxis;
  
  // Clasificación sísmica estimada (Escala Mercalli / Shindo)
  if (maxMag < 0.015) {
    strcpy(ev.intensity, "Micro-sismo (I)");
  } else if (maxMag < 0.035) {
    strcpy(ev.intensity, "Leve (II-III)");
  } else if (maxMag < 0.080) {
    strcpy(ev.intensity, "Moderado (IV-V)");
  } else if (maxMag < 0.180) {
    strcpy(ev.intensity, "Fuerte (VI-VII)");
  } else {
    strcpy(ev.intensity, "Severo (VIII+)");
  }
  
  eventsLog[nextEventIndex] = ev;
  nextEventIndex = (nextEventIndex + 1) % MAX_EVENTS;
  if (eventCount < MAX_EVENTS) eventCount++;
  
  saveEventsToNVS();
  
  // Difundir evento de inmediato vía WebSocket para que la interfaz lo agregue en tiempo real
  String evJson = "{\"type\":\"event\",\"id\":" + String(ev.id) + 
                  ",\"time\":\"" + String(ev.time) + 
                  "\",\"pga\":" + String(ev.pga, 3) + 
                  ",\"dur\":" + String(ev.duration, 1) + 
                  ",\"axis\":\"" + String(ev.axis) + 
                  "\",\"intensity\":\"" + String(ev.intensity) + "\"}";
  webSocket.broadcastTXT(evJson);
  if (mqttClient.connected()) {
    mqttClient.publish(MQTT_TOPIC_EVENTS, evJson.c_str(), true);
  }
  
  Serial.printf("[SISMO] #%d %s %.3fg %s\n", ev.id, ev.time, ev.pga, ev.intensity);
}

void handleAPIGetEvents() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String json = "[";
  for (int i = 0; i < eventCount; i++) {
    // El más reciente primero
    int idx = (nextEventIndex - 1 - i + MAX_EVENTS) % MAX_EVENTS;
    SeismicEvent &ev = eventsLog[idx];
    if (i > 0) json += ",";
    json += "{";
    json += "\"id\":" + String(ev.id) + ",";
    json += "\"time\":\"" + String(ev.time) + "\",";
    json += "\"pga\":" + String(ev.pga, 3) + ",";
    json += "\"dur\":" + String(ev.duration, 1) + ",";
    json += "\"axis\":\"" + String(ev.axis) + "\",";
    json += "\"intensity\":\"" + String(ev.intensity) + "\"";
    json += "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleAPIClearEvents() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  eventCount = 0;
  nextEventIndex = 0;
  memset(eventsLog, 0, sizeof(eventsLog));
  saveEventsToNVS();
  Serial.println("[HISTORIAL] Limpiado");
  server.send(200, "application/json", "{\"status\":\"cleared\"}");
}

void handleAPIIfttt() {
  // Endpoint: /api/ifttt?key=IFTTT_KEY_AQUÍ&event=sismo_alert
  if (server.hasArg("key")) {
    iftttKey = server.arg("key");
  }
  if (server.hasArg("event")) {
    iftttEvent = server.arg("event");
  }

  // Guardar en Preferences
  preferences.begin("seismic-alarm", false);
  preferences.putString("iftttKey", iftttKey);
  preferences.putString("iftttEvent", iftttEvent);
  preferences.end();

  Serial.println("[IFTTT] Guardado");
  server.send(200, "application/json", "{\"status\":\"success\",\"event\":\"" + iftttEvent + "\"}");
}

// --- OBTENER TIEMPO DE OPERACIÓN (UPTIME) ---
String getFormattedUptime() {
  unsigned long ms = millis();
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  seconds = seconds % 60;
  minutes = minutes % 60;
  
  char buffer[12];
  sprintf(buffer, "%02lu:%02lu:%02lu", hours, minutes, seconds);
  return String(buffer);
}

void handleAPIGetConfig() {
  String json = "{";
  json += "\"threshold\":" + String(threshold, 3) + ",";
  json += "\"duration\":" + String(alarmDuration) + ",";
  json += "\"wifiSsid\":\"" + wifiSsid + "\",";
  json += "\"iftttKey\":\"" + iftttKey + "\",";
  json += "\"iftttEvent\":\"" + iftttEvent + "\",";
  json += "\"sinricAppKey\":\"" + sinricAppKey + "\",";
  json += "\"sinricAppSecret\":\"" + sinricAppSecret + "\",";
  json += "\"sinricDeviceId\":\"" + sinricDeviceId + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleAPIWifi() {
  if (server.hasArg("ssid")) {
    wifiSsid = server.arg("ssid");
  }
  if (server.hasArg("password")) {
    wifiPass = server.arg("password");
  }

  preferences.begin("seismic-alarm", false);
  preferences.putString("wifiSsid", wifiSsid);
  preferences.putString("wifiPass", wifiPass);
  preferences.end();

  Serial.println("[WiFi] Guardado");
  server.send(200, "application/json", "{\"status\":\"success\"}");
  
  delay(1000);
  ESP.restart();
}

void handleAPISinricConfig() {
  if (server.hasArg("appKey")) {
    sinricAppKey = server.arg("appKey");
  }
  if (server.hasArg("appSecret")) {
    sinricAppSecret = server.arg("appSecret");
  }
  if (server.hasArg("deviceId")) {
    sinricDeviceId = server.arg("deviceId");
  }

  preferences.begin("seismic-alarm", false);
  preferences.putString("sinricAppKey", sinricAppKey);
  preferences.putString("sinricAppSecret", sinricAppSecret);
  preferences.putString("sinricDeviceId", sinricDeviceId);
  preferences.end();

  Serial.println("[Sinric] Guardado");
  server.send(200, "application/json", "{\"status\":\"success\"}");
  
  delay(1000);
  ESP.restart();
}

// =============================================================================
// OTA (Over-The-Air) — Actualizacion inalambrica de firmware
// =============================================================================

// Pagina HTML para subir firmware desde el navegador
void handleOTAPage() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  static const char otaHtml[] PROGMEM = 
    "<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>OTA Sismografo</title><style>"
    "body{background:#0e0a1a;color:#f3f0f7;font-family:sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0;text-align:center;}"
    ".box{background:rgba(255,255,255,.05);border:1px solid rgba(255,255,255,.1);border-radius:16px;padding:30px 20px;max-width:380px;width:90%;}"
    "h2{color:#bb86fc;margin-bottom:8px;font-size:18px;}"
    "p{color:#9b93b3;font-size:12px;line-height:1.4;margin-bottom:16px;}"
    "input[type=file]{margin-bottom:14px;color:#bb86fc;font-size:12px;}"
    "button{background:#bb86fc;color:#120326;border:none;padding:10px 20px;border-radius:8px;font-weight:700;cursor:pointer;width:100%;font-size:13px;}"
    "#st{font-size:12px;color:#00e5ff;margin-top:10px;}"
    "</style></head><body><div class='box'><h2>&#128225; Actualizar Firmware</h2>"
    "<p>Sube el archivo <strong>seismic_alarm.ino.bin</strong> para actualizar via WiFi sin desconectar el equipo.</p>"
    "<form id='f' method='POST' action='/update' enctype='multipart/form-data'>"
    "<input type='file' id='b' name='update' accept='.bin'><br>"
    "<button type='submit' id='btn'>Subir Firmware</button></form>"
    "<div id='st'></div><p style='margin-top:18px;'><a href='/' style='color:#bb86fc;text-decoration:none;'>&larr; Volver</a></p></div>"
    "<script>"
    "document.getElementById('f').onsubmit=function(e){"
    "e.preventDefault();var fi=document.getElementById('b').files[0];if(!fi){alert('Elige un .bin');return;}"
    "document.getElementById('btn').disabled=true;document.getElementById('st').innerText='Subiendo...';"
    "var x=new XMLHttpRequest();"
    "x.upload.onprogress=function(ev){if(ev.lengthComputable){document.getElementById('st').innerText='Subiendo: '+Math.round(ev.loaded/ev.total*100)+'%';}};"
    "x.onload=function(){document.getElementById('st').innerText=x.responseText;if(x.responseText.indexOf('OK')>=0)setTimeout(function(){window.location='/';},4500);};"
    "x.onerror=function(){document.getElementById('st').innerText='Error';};"
    "x.open('POST','/update');x.send(new FormData(this));"
    "};</script></body></html>";
  server.send_P(200, "text/html", otaHtml);
}

// Manejador de subida de firmware (.bin) via HTTP POST multipart
void handleOTAUpload() {
  HTTPUpload& upload = server.upload();
  static size_t bytesTransferred = 0;
  
  if (upload.status == UPLOAD_FILE_START) {
    bytesTransferred = 0;
    Serial.println("[OTA] Start");
    // Pausar sistemas para no interferir durante OTA
    if (alarmActive) deactivateAlarm();
    strip.fill(strip.Color(0, 0, 150));
    strip.show();
    
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Escribir chunk de datos al flash
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
    bytesTransferred += upload.currentSize;
    // Progreso en LEDs: Un LED se enciende en secuencia por cada ~40KB descargados
    int ledsOn = (bytesTransferred / 40000) % NUM_LEDS; 
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, i == ledsOn ? strip.Color(0, 50, 255) : strip.Color(0, 0, 50));
    }
    strip.show();
    
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("[OTA] OK: %u B\n", bytesTransferred);
      strip.fill(strip.Color(0, 255, 0));
      strip.show();
    } else {
      Update.printError(Serial);
      strip.fill(strip.Color(255, 0, 0));
      strip.show();
    }
  }
}

// =============================================================================
// GESTIÓN MQTT CLOUD (broker.emqx.io - 24/7 sin PC)
// =============================================================================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
  message.trim();
  Serial.printf("[MQTT RX] %s -> %s\n", topic, message.c_str());

  if (String(topic) == MQTT_TOPIC_CMD) {
    if (message.startsWith("threshold:")) {
      float t = message.substring(10).toFloat();
      if (t >= 0.005 && t <= 1.0) {
        threshold = t;
        preferences.begin("seismic-alarm", false);
        preferences.putFloat("threshold", threshold);
        preferences.end();
        Serial.printf("[MQTT] Umbral remoto actualizado: %.3fg\n", threshold);
      }
    } else if (message == "calibrate") {
      handleAPICalibrate();
    }
  }
}

void checkMqtt() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttReconnect > 5000) {
      lastMqttReconnect = now;
      String clientId = "ESP32_Sismo_" + String((uint32_t)ESP.getEfuseMac(), HEX);
      Serial.print("[MQTT] Conectando a broker.emqx.io... ");
      // Conectar con LWT (Last Will and Testament) para avisar si el sensor se apaga
      if (mqttClient.connect(clientId.c_str(), MQTT_TOPIC_STATUS, 1, true, "offline")) {
        Serial.println("¡Conectado!");
        mqttClient.publish(MQTT_TOPIC_STATUS, "online", true);
        mqttClient.subscribe(MQTT_TOPIC_CMD);
      } else {
        Serial.printf("Fallo rc=%d\n", mqttClient.state());
      }
    }
  } else {
    mqttClient.loop();
  }
}

