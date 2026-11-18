/*
 * PRÁCTICA 3 – Control Remoto Completo del Robot
 * Robot ESP32 – Ingeniería en Sistemas Computacionales
 *
 * Descripción:
 *   Firmware completo e integrado del robot móvil diferencial.
 *   Incluye: servidor web, servidor WebSocket, control de motores
 *   PWM, LEDs indicadores de estado y monitor de tensión de batería
 *   mediante el ADC del ESP32.
 *
 * Archivos necesarios en /data (LittleFS):
 *   - index.html   (interfaz web principal)
 *   - style.css    (estilos visuales)
 *   - app.js       (cliente WebSocket en JavaScript)
 *
 * Conexiones ESP32 → L298N (igual que la Práctica 2):
 *   GPIO 26 → IN1    GPIO 27 → IN2   (Motor A – Rueda Derecha)
 *   GPIO 14 → IN3    GPIO 32 → IN4   (Motor B – Rueda Izquierda)
 *   GPIO 25 → ENA    GPIO 33 → ENB   (PWM; retirar jumpers ENA/ENB)
 *   GND     → GND   (tierra común OBLIGATORIA)
 *
 * Monitor de batería (2 celdas Li-ion, 6.0–8.4 V):
 *   Vbat ──[47 kΩ]──┬── GPIO 34 ──┐
 *                   [10 kΩ]   [100 nF]  (capacitor recomendado:
 *                    │            │      estabiliza la lectura)
 *                   GND ─────────┘
 *   GPIO 34 pertenece al ADC1, que funciona aunque el WiFi esté
 *   activo (el ADC2 no se puede usar con WiFi).
 *
 * Dependencias:
 *   - ESP32 Board Package  (Espressif Systems, v3.x)
 *   - ESPAsyncWebServer    (ESP32Async – Gestor de librerías)
 *   - AsyncTCP             (ESP32Async – Gestor de librerías)
 */

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#if ESP_ARDUINO_VERSION_MAJOR < 3
#error "Esta práctica requiere el ESP32 Board Package v3.x (API LEDC nueva)."
#endif

// ── Configuración por equipo ──────────────────────────────────────
#define NUM_EQUIPO  "01"   // Cambiar: 01, 02, 03...
#define CANAL_WIFI  1      // Repartir entre equipos: 1, 6 u 11

const char* SSID     = "RobotESP32_E" NUM_EQUIPO;   // p. ej. RobotESP32_E01
const char* PASSWORD = "robot1234";                  // Mín. 8 caracteres

// ── Pines de dirección del L298N ─────────────────────────────────
#define IN1  26
#define IN2  27
#define IN3  14
#define IN4  32

// ── Pines PWM del L298N ──────────────────────────────────────────
#define ENA  25
#define ENB  33

// ── LEDs indicadores ─────────────────────────────────────────────
#define LED_ENCENDIDO  4    // LED rojo: sistema energizado
#define LED_CONECTADO  2    // LED verde: hay al menos un cliente WebSocket

// ── Monitor de batería ───────────────────────────────────────────
// Factor del divisor: 10/(47+10) = 0.175 → Vmáx en GPIO34 = 8.4 × 0.175 ≈ 1.47 V
#define BAT_PIN          34
#define BAT_DIVISOR      5.7f    // Inverso del factor del divisor (57/10)
#define BAT_BAJA         6.8f    // Umbral de advertencia (V)
#define BAT_SIN_BATERIA  2.0f    // Por debajo de esto se asume que no hay
                                 // batería conectada (p. ej. solo USB)
const uint32_t INTERVALO_BAT = 10000;   // Enviar lectura cada 10 s

// ── Configuración PWM ─────────────────────────────────────────────
#define PWM_FREQ       1000
#define PWM_RES        8

// ── Velocidades (0–255) ───────────────────────────────────────────
#define VEL_ALTA       204   // 80 %
#define VEL_GIRO       178   // 70 %
#define VEL_PUNTO      153   // 60 %

// ── Seguridad ─────────────────────────────────────────────────────
//    app.js reenvía el comando cada 200 ms mientras el botón está
//    presionado. Si no llega nada en este tiempo, el robot se detiene.
#define TIEMPO_SEGURIDAD_MS  500

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ── Comunicación entre el manejador WebSocket y loop() ───────────
//    El manejador WebSocket corre en la tarea de AsyncTCP. Ahí no se
//    deben mover motores ni usar delay(); solo se dejan avisos que
//    loop() atiende.
volatile char          comandoPendiente = 0;
volatile unsigned long ultimoComandoMs  = 0;
volatile bool          pedirBateria     = false;
char                   movimientoActual = 'S';
uint32_t               ultimaLecturaBat = 0;

//
//  MÓDULO DE CONTROL DE MOTORES
//

void initMotores() {
  const int pines[] = {IN1, IN2, IN3, IN4};
  for (int p : pines) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
  // API LEDC del core 3.x: se asocia el pin directamente (sin canales)
  if (!ledcAttach(ENA, PWM_FREQ, PWM_RES) ||
      !ledcAttach(ENB, PWM_FREQ, PWM_RES)) {
    Serial.println("[ERROR] No se pudo configurar el PWM.");
    while (true) delay(1000);
  }
}

void setMotor(int i1, int i2, int i3, int i4, int pA, int pB) {
  digitalWrite(IN1, i1); digitalWrite(IN2, i2);
  digitalWrite(IN3, i3); digitalWrite(IN4, i4);
  ledcWrite(ENA, pA);    // En el core 3.x ledcWrite recibe el PIN
  ledcWrite(ENB, pB);
}

void detener()     { setMotor(0,0,0,0, 0,         0);         }
void avanzar()     { setMotor(1,0,1,0, VEL_ALTA,  VEL_ALTA);  }
void retroceder()  { setMotor(0,1,0,1, VEL_ALTA,  VEL_ALTA);  }
void girarDer()    { setMotor(0,0,1,0, 0,         VEL_GIRO);  }
void girarIzq()    { setMotor(1,0,0,0, VEL_GIRO,  0);         }
void girarPunto()  { setMotor(1,0,0,1, VEL_PUNTO, VEL_PUNTO); }

// Ejecuta un movimiento. Solo se llama desde setup() o loop().
void aplicarMovimiento(char c) {
  switch (c) {
    case 'F': avanzar();    break;
    case 'B': retroceder(); break;
    case 'R': girarDer();   break;
    case 'L': girarIzq();   break;
    case 'P': girarPunto(); break;
    default:  detener(); c = 'S'; break;
  }
  if (c != movimientoActual) {
    movimientoActual = c;
    Serial.printf("[CMD] Movimiento: %c\n", c);
    ws.textAll(String("MOV:") + c);   // Confirmar a la interfaz web
  }
}

//
//  MÓDULO DE MONITOR DE BATERÍA
//

float leerBateria() {
  // analogReadMilliVolts() usa la calibración de fábrica del chip.
  // Es mucho más exacta que (analogRead / 4095) × 3.3, porque el ADC
  // del ESP32 no es lineal; ese error se multiplicaría ×5.7.
  uint32_t sumaMv = 0;
  const int N = 16;
  for (int i = 0; i < N; i++) {
    sumaMv += analogReadMilliVolts(BAT_PIN);
    delayMicroseconds(200);
  }
  float vPin = (sumaMv / (float)N) / 1000.0f;   // Tensión en el pin (V)
  return vPin * BAT_DIVISOR;                    // Tensión de la batería (V)
}

void enviarEstadoBateria() {
  float v = leerBateria();

  if (v < BAT_SIN_BATERIA) {
    // Sin batería (o divisor desconectado): no lanzar alertas falsas
    ws.textAll("BAT:NA");
    Serial.printf("[BAT] Sin batería detectada (%.2f V)\n", v);
    return;
  }

  char msg[32];
  // Formato: 'BAT:7.85' – interpretado por app.js
  snprintf(msg, sizeof(msg), "BAT:%.2f", v);
  ws.textAll(msg);
  Serial.printf("[BAT] %.2f V\n", v);

  if (v <= BAT_BAJA) {
    ws.textAll("ALERTA:Batería baja – carga el robot pronto");
    Serial.println("[BAT] ¡ADVERTENCIA! Batería baja.");
  }
}

//
//  MANEJADOR WebSocket
//

void onWsEvent(AsyncWebSocket *srv, AsyncWebSocketClient *client,
               AwsEventType type, void *arg,
               uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Cliente #%u conectado\n", client->id());
    client->text(String("MOV:") + movimientoActual);
    pedirBateria = true;     // loop() enviará la lectura de batería

  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Cliente #%u desconectado\n", client->id());
    comandoPendiente = 'S';  // Seguridad: detener robot al desconectar

  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 &&
        info->len == len && info->opcode == WS_TEXT) {

      // Copiar exactamente 'len' bytes (sin escribir data[len])
      String cmd;
      cmd.reserve(len);
      for (size_t i = 0; i < len; i++) cmd += (char)data[i];
      cmd.trim();

      if (cmd.length() == 1 && strchr("FBRLPS", cmd[0]) != nullptr) {
        ultimoComandoMs  = millis();   // Primero la marca de tiempo...
        comandoPendiente = cmd[0];     // ...luego el comando
      } else {
        Serial.printf("[CMD] Comando desconocido: '%s'\n", cmd.c_str());
        client->text("ALERTA:Comando no reconocido");
      }
    }
  }
}

//
//  SETUP
//

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n= Robot ESP32 – Práctica 3 (Completo) =");

  // LEDs de estado
  pinMode(LED_ENCENDIDO, OUTPUT);  digitalWrite(LED_ENCENDIDO, HIGH);
  pinMode(LED_CONECTADO, OUTPUT);  digitalWrite(LED_CONECTADO, LOW);

  // Motores
  initMotores();
  detener();

  // ADC para batería: atenuación de 11 dB (rango útil ~0.15–3.1 V)
  analogReadResolution(12);
  analogSetPinAttenuation(BAT_PIN, ADC_11db);

  // Sistema de archivos
  if (!LittleFS.begin()) {
    Serial.println("[ERROR] No se pudo montar LittleFS.");
    Serial.println("        Sube la carpeta /data con");
    Serial.println("        'Upload LittleFS to Pico/ESP8266/ESP32'.");
    while (true) delay(1000);
  }
  const char* archivos[] = {"/index.html", "/style.css", "/app.js"};
  for (const char* f : archivos) {
    if (!LittleFS.exists(f)) Serial.printf("[AVISO] Falta %s en LittleFS\n", f);
  }

  // WiFi Access Point
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(SSID, PASSWORD, CANAL_WIFI)) {
    Serial.println("[ERROR] No se pudo crear el Access Point.");
    while (true) delay(1000);
  }
  delay(500);
  Serial.printf("[WiFi] Red: %s (canal %d)\n", SSID, CANAL_WIFI);
  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.softAPIP());

  // Servidor web + WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.onNotFound([](AsyncWebServerRequest *r) {
    r->send(404, "text/plain", "No encontrado");
  });
  server.begin();
  Serial.print("[OK] Robot listo en http://");
  Serial.println(WiFi.softAPIP());
}

//
//  LOOP
//

void loop() {
  uint32_t ahora = millis();

  // 1. Ejecutar el comando recibido por WebSocket
  char c = comandoPendiente;
  if (c != 0) {
    comandoPendiente = 0;
    aplicarMovimiento(c);
  }

  // 2. Seguridad: detener si dejaron de llegar comandos
  if (movimientoActual != 'S' &&
      millis() - ultimoComandoMs > TIEMPO_SEGURIDAD_MS) {
    Serial.println("[SEGURIDAD] Sin comandos recientes: deteniendo.");
    aplicarMovimiento('S');
  }

  // 3. LED de conexión: encendido mientras haya algún cliente
  digitalWrite(LED_CONECTADO, ws.count() > 0 ? HIGH : LOW);

  // 4. Batería: al conectarse un cliente y luego cada INTERVALO_BAT
  if (pedirBateria || ahora - ultimaLecturaBat >= INTERVALO_BAT) {
    pedirBateria     = false;
    ultimaLecturaBat = ahora;
    if (ws.count() > 0) enviarEstadoBateria();
  }

  // 5. Limpiar conexiones WebSocket cerradas (una vez por segundo)
  static uint32_t ultimaLimpieza = 0;
  if (ahora - ultimaLimpieza >= 1000) {
    ws.cleanupClients();
    ultimaLimpieza = ahora;
  }

  delay(10);
}
