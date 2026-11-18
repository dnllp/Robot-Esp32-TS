/*
 * PRÁCTICA 1 – Control de LED desde la Web
 * Robot ESP32 – Ingeniería en Sistemas Computacionales
 *
 * Descripción:
 *   El ESP32 genera una red WiFi propia (Access Point) y levanta
 *   un servidor web que sirve la interfaz HTML almacenada en
 *   LittleFS. El usuario controla un LED físico en GPIO 2
 *   enviando comandos 'ON' y 'OFF' a través de WebSockets.
 *
 * Dependencias (instalar en Arduino IDE antes de compilar):
 *   - ESP32 Board Package  (Espressif Systems, v3.x)
 *   - ESPAsyncWebServer    (ESP32Async – Gestor de librerías)
 *   - AsyncTCP             (ESP32Async – Gestor de librerías)
 *   LittleFS ya viene incluido con el Board Package de ESP32.
 *   NOTA: NO usar las versiones de me-no-dev; están abandonadas
 *   y no compilan con el core ESP32 3.x.
 *
 * Plugin para subir archivos (Arduino IDE 2.x):
 *   arduino-littlefs-upload (archivo .vsix) copiado en
 *   ~/.arduinoIDE/plugins/  (crear la carpeta si no existe)
 *   y reiniciar el IDE.
 *
 * Pasos para cargar:
 *   1. Seleccionar la tarjeta "ESP32 Dev Module".
 *   2. Cambiar NUM_EQUIPO y CANAL_WIFI (ver abajo).
 *   3. Cerrar el Monitor Serial (si está abierto falla la carga).
 *   4. Subir el sistema de archivos: Ctrl+Shift+P →
 *      "Upload LittleFS to Pico/ESP8266/ESP32"
 *      (la carpeta /data debe contener index.html).
 *   5. Compilar y subir este sketch (Sketch → Subir).
 *   6. Abrir Monitor Serial a 115200 baud.
 *   7. Conectarse a la red 'PracticaLED_Exx' con contraseña 'practica1'.
 *      En el celular, desactivar datos móviles.
 *   8. Navegar a http://192.168.4.1 en el navegador.
 */

// ── Librerías necesarias ──────────────────────────────────────────
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// ── Configuración por equipo ──────────────────────────────────────
#define NUM_EQUIPO  "01"   // Cambiar: 01, 02, 03...
#define CANAL_WIFI  1      // Repartir entre equipos: 1, 6 u 11

// ── Credenciales de la red WiFi del ESP32 (Access Point) ─────────
const char* SSID     = "PracticaLED_E" NUM_EQUIPO;  // p. ej. PracticaLED_E01
const char* PASSWORD = "practica1";                  // Mín. 8 caracteres

// ── Definición de pines ───────────────────────────────────────────
#define LED_PIN 2   // GPIO 2: LED integrado en el ESP32 DevKit clásico.
                    // En placas C3/S3 el LED integrado está en otro pin.
                    // También puedes conectar un LED externo con
                    // resistencia de 330 Ω a cualquier GPIO libre.

// ── Estado del LED ────────────────────────────────────────────────
bool ledEncendido = false;   // Fuente única de verdad del estado

// ── Instancias del servidor HTTP y del WebSocket ──────────────────
AsyncWebServer server(80);      // Servidor HTTP en el puerto 80
AsyncWebSocket ws("/ws");       // Endpoint WebSocket en /ws

// ── Texto de estado que se envía a los clientes ──────────────────
const char* textoEstado() {
  return ledEncendido ? "LED ENCENDIDO" : "LED APAGADO";
}

// ── Cambiar el LED y notificar a todos los clientes ──────────────
void fijarLed(bool encender) {
  ledEncendido = encender;
  digitalWrite(LED_PIN, ledEncendido ? HIGH : LOW);
  ws.textAll(textoEstado());   // Notificar a TODOS los clientes
  Serial.printf("[LED] %s\n", ledEncendido ? "Encendido" : "Apagado");
}

// ── Manejador de eventos WebSocket ───────────────────────────────
//    Se ejecuta automáticamente cada vez que ocurre un evento
//    en cualquier conexión WebSocket activa.
void onWsEvent(AsyncWebSocket       *server,
               AsyncWebSocketClient *client,
               AwsEventType          type,
               void                 *arg,
               uint8_t              *data,
               size_t                len) {

  switch (type) {

    case WS_EVT_CONNECT:
      Serial.printf("[WS] Cliente #%u conectado desde: %s\n",
                    client->id(),
                    client->remoteIP().toString().c_str());
      // Enviar el estado actual del LED al cliente recién conectado
      client->text(textoEstado());
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf("[WS] Cliente #%u desconectado\n", client->id());
      break;

    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      // Verificar que es un mensaje de texto completo (no fragmentado)
      if (info->final && info->index == 0 &&
          info->len == len && info->opcode == WS_TEXT) {

        // Copiar exactamente 'len' bytes. No se escribe data[len],
        // porque esa posición está fuera del búfer recibido.
        String cmd;
        cmd.reserve(len);
        for (size_t i = 0; i < len; i++) {
          cmd += (char)data[i];
        }
        cmd.trim();   // Eliminar espacios y saltos de línea

        Serial.printf("[WS] Comando recibido: '%s'\n", cmd.c_str());

        if (cmd == "ON") {
          fijarLed(true);
        } else if (cmd == "OFF") {
          fijarLed(false);
        } else {
          Serial.printf("[WS] Comando desconocido: '%s'\n", cmd.c_str());
          client->text("Comando no reconocido. Use 'ON' u 'OFF'.");
        }
      }
      break;
    }

    case WS_EVT_ERROR:
      Serial.printf("[WS] Error en cliente #%u\n", client->id());
      break;

    default:
      break;
  }
}

// ── Configuración inicial (se ejecuta una sola vez al encender) ───
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n= Práctica 1: Control de LED vía WebSocket =");

  // Configurar el pin del LED como salida y apagarlo inicialmente
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  ledEncendido = false;

  // ── Montar el sistema de archivos LittleFS ──────────────────────
  if (!LittleFS.begin()) {
    Serial.println("[ERROR] No se pudo montar LittleFS.");
    Serial.println("        Verifica que subiste la carpeta /data con");
    Serial.println("        'Upload LittleFS to Pico/ESP8266/ESP32'.");
    while (true) delay(1000);   // Detener ejecución con error
  }
  if (!LittleFS.exists("/index.html")) {
    Serial.println("[AVISO] LittleFS montado, pero no existe /index.html.");
  }
  Serial.println("[OK] LittleFS montado correctamente.");

  // ── Configurar el ESP32 como Punto de Acceso WiFi ───────────────
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(SSID, PASSWORD, CANAL_WIFI)) {
    Serial.println("[ERROR] No se pudo crear el Access Point.");
    while (true) delay(1000);
  }
  delay(500);   // Esperar a que el AP se inicialice completamente

  Serial.printf("[WiFi] Access Point activo. SSID: %s (canal %d)\n",
                SSID, CANAL_WIFI);
  Serial.print("[WiFi] Dirección IP del ESP32: ");
  Serial.println(WiFi.softAPIP());   // Normalmente 192.168.4.1

  // ── Registrar el manejador de WebSocket en el servidor ──────────
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // ── Configurar rutas del servidor HTTP ──────────────────────────
  // Servir los archivos de LittleFS bajo la ruta raíz '/'.
  // index.html se sirve automáticamente al acceder a '/'.
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // Ruta de respaldo: 404 para rutas no encontradas
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Página no encontrada.");
  });

  // ── Iniciar el servidor HTTP ─────────────────────────────────────
  server.begin();
  Serial.print("[OK] Servidor web iniciado en http://");
  Serial.println(WiFi.softAPIP());
  Serial.printf("     Conecta tu dispositivo a la red '%s'\n", SSID);
}

// ── Bucle principal (se ejecuta continuamente) ────────────────────
void loop() {
  // ESPAsyncWebServer maneja todo de forma asíncrona.
  // Solo hay que limpiar las conexiones WebSocket cerradas.
  ws.cleanupClients();
  delay(100);   // Ceder tiempo al sistema operativo del ESP32
}
