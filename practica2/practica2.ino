/*
 * PRÁCTICA 2 – Movimiento Básico de Motores
 * Robot ESP32 – Ingeniería en Sistemas Computacionales
 *
 * Descripción:
 *   Controla dos motores DC mediante el driver L298N usando
 *   señales PWM generadas por el ESP32. En la primera fase
 *   (Paso 3 de la práctica) los motores se prueban directamente
 *   sin WebSocket. En la segunda fase se integra el control
 *   remoto vía WebSocket con la interfaz web.
 *
 * Conexiones ESP32 → L298N (ver Tabla 3.2 del Cap. III):
 *   GPIO 26 → IN1    GPIO 27 → IN2   (Motor A – Rueda Derecha)
 *   GPIO 14 → IN3    GPIO 32 → IN4   (Motor B – Rueda Izquierda)
 *   GPIO 25 → ENA    GPIO 33 → ENB   (PWM de velocidad)
 *   GND     → GND   (tierra común OBLIGATORIA)
 *
 * Dependencias:
 *   - ESP32 Board Package  (Espressif Systems, v3.x)
 *   - ESPAsyncWebServer    (ESP32Async – Gestor de librerías)
 *   - AsyncTCP             (ESP32Async – Gestor de librerías)
 *
 * Fases de la práctica:
 *   FASE 1 (Paso 3): poner MODO_PRUEBA en 1. Los motores ejecutan
 *                    una secuencia fija, sin WiFi.
 *                    ¡Levantar el robot para que las ruedas giren
 *                    en el aire!
 *   FASE 2:          poner MODO_PRUEBA en 0. Control remoto vía
 *                    WebSocket desde la interfaz web.
 */

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#if ESP_ARDUINO_VERSION_MAJOR < 3
#error "Esta práctica requiere el ESP32 Board Package v3.x (API LEDC nueva)."
#endif

// ── Selección de fase ─────────────────────────────────────────────
#define MODO_PRUEBA  0     // 1 = FASE 1 (prueba directa), 0 = FASE 2 (WiFi)

// ── Configuración por equipo ──────────────────────────────────────
#define NUM_EQUIPO  "01"   // Cambiar: 01, 02, 03...
#define CANAL_WIFI  1      // Repartir entre equipos: 1, 6 u 11

const char* SSID     = "RobotP2_E" NUM_EQUIPO;   // p. ej. RobotP2_E01
const char* PASSWORD = "practica2";               // Mín. 8 caracteres

// ── Pines de control del L298N ────────────────────────────────────
#define IN1  26   // Dirección Motor A (rueda derecha)
#define IN2  27
#define IN3  14   // Dirección Motor B (rueda izquierda)
#define IN4  32
#define ENA  25   // PWM Motor A
#define ENB  33   // PWM Motor B

// ── Configuración PWM ─────────────────────────────────────────────
#define PWM_FREQ        1000   // Frecuencia: 1 kHz
#define PWM_RESOLUTION     8   // Resolución: 8 bits (valores 0–255)

// ── Velocidades predefinidas (duty cycle de 0 a 255) ─────────────
//    El L298N pierde ~2 V internamente; si con VEL_BAJA los motores
//    no arrancan, aumentar el valor.
#define VEL_ALTA   204   // 80 % – avance normal
#define VEL_MEDIA  178   // 70 % – giros
#define VEL_BAJA   128   // 50 % – giro en su eje

// ── Seguridad ─────────────────────────────────────────────────────
//    La interfaz web reenvía el comando cada 200 ms mientras el botón
//    está presionado. Si pasan más de TIEMPO_SEGURIDAD_MS sin recibir
//    nada (se perdió el WiFi, se cerró el navegador, etc.), el robot
//    se detiene solo.
#define TIEMPO_SEGURIDAD_MS  500

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ── Comunicación entre el manejador WebSocket y loop() ───────────
//    El manejador WebSocket corre en otra tarea (AsyncTCP). Para no
//    mover los motores desde dos tareas a la vez, el manejador solo
//    deja el comando aquí y loop() es quien lo ejecuta.
volatile char          comandoPendiente = 0;
volatile unsigned long ultimoComandoMs  = 0;
char                   movimientoActual = 'S';

// ── Inicialización de pines y PWM ────────────────────────────────
void initMotores() {
  const int pinesDir[] = {IN1, IN2, IN3, IN4};
  for (int p : pinesDir) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }
  // API LEDC del core 3.x: se asocia directamente el pin,
  // ya no se manejan canales (ledcSetup/ledcAttachPin se eliminaron).
  if (!ledcAttach(ENA, PWM_FREQ, PWM_RESOLUTION) ||
      !ledcAttach(ENB, PWM_FREQ, PWM_RESOLUTION)) {
    Serial.println("[ERROR] No se pudo configurar el PWM.");
    while (true) delay(1000);
  }
  ledcWrite(ENA, 0);
  ledcWrite(ENB, 0);
  Serial.println("[Motor] Pines y PWM inicializados.");
}

// ── Función base de control de motores ───────────────────────────
//    Argumentos: estados lógicos de IN1–IN4 y duty cycle de A y B
void setMotor(int in1, int in2, int in3, int in4,
              int pwmA, int pwmB) {
  digitalWrite(IN1, in1);  digitalWrite(IN2, in2);
  digitalWrite(IN3, in3);  digitalWrite(IN4, in4);
  ledcWrite(ENA, pwmA);    // En el core 3.x ledcWrite recibe el PIN
  ledcWrite(ENB, pwmB);
}

// ── Funciones de movimiento ───────────────────────────────────────
//    Motor A = rueda derecha, Motor B = rueda izquierda
void detener()    { setMotor(0,0,0,0, 0,         0);         }
void avanzar()    { setMotor(1,0,1,0, VEL_ALTA,  VEL_ALTA);  }
void retroceder() { setMotor(0,1,0,1, VEL_ALTA,  VEL_ALTA);  }
void girarDer()   { setMotor(0,0,1,0, 0,         VEL_MEDIA); } // solo rueda izq.
void girarIzq()   { setMotor(1,0,0,0, VEL_MEDIA, 0);         } // solo rueda der.
void girarPunto() { setMotor(1,0,0,1, VEL_BAJA,  VEL_BAJA);  } // en su eje, a la izq.

// ── Ejecutar un movimiento (solo se llama desde loop/setup) ──────
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
    Serial.printf("[Motor] Movimiento: %c\n", c);
    ws.textAll(String("MOV:") + c);   // Informar a la interfaz web
  }
}

// ── Manejador de eventos WebSocket ───────────────────────────────
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg,
               uint8_t *data, size_t len) {

  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Cliente #%u conectado\n", client->id());
    client->text(String("MOV:") + movimientoActual);

  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Cliente #%u desconectado\n", client->id());
    comandoPendiente = 'S';   // ¡IMPORTANTE! Detener al perder conexión

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
        client->text("ERR:Comando no reconocido");
      }
    }
  }
}

// ── Secuencia de prueba (FASE 1) ──────────────────────────────────
void pruebaMotores() {
  Serial.println("[TEST] Prueba directa de motores (sin WiFi).");
  Serial.println("[TEST] ¡Mantén el robot levantado!");
  while (true) {
    Serial.println("Avanzando 2 s");          avanzar();    delay(2000);
    Serial.println("Detenido 1 s");           detener();    delay(1000);
    Serial.println("Retrocediendo 2 s");      retroceder(); delay(2000);
    Serial.println("Detenido 1 s");           detener();    delay(1000);
    Serial.println("Girando derecha 1 s");    girarDer();   delay(1000);
    Serial.println("Detenido 0.5 s");         detener();    delay(500);
    Serial.println("Girando izquierda 1 s");  girarIzq();   delay(1000);
    Serial.println("Detenido 0.5 s");         detener();    delay(500);
    Serial.println("Girando en su eje 1 s");  girarPunto(); delay(1000);
    Serial.println("Detenido 2 s");           detener();    delay(2000);
  }
}

// ── setup() ───────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n= Práctica 2: Control de Motores =");

  initMotores();
  detener();

#if MODO_PRUEBA
  pruebaMotores();   // No regresa nunca
#endif

  // ── FASE 2: Control vía WiFi y WebSocket ────────────────────────
  if (!LittleFS.begin()) {
    Serial.println("[ERROR] No se pudo montar LittleFS.");
    Serial.println("        Sube la carpeta /data con");
    Serial.println("        'Upload LittleFS to Pico/ESP8266/ESP32'.");
    while (true) delay(1000);
  }
  if (!LittleFS.exists("/index.html")) {
    Serial.println("[AVISO] LittleFS montado, pero no existe /index.html.");
  }

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(SSID, PASSWORD, CANAL_WIFI)) {
    Serial.println("[ERROR] No se pudo crear el Access Point.");
    while (true) delay(1000);
  }
  delay(500);
  Serial.printf("[WiFi] Red: %s (canal %d)\n", SSID, CANAL_WIFI);
  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.softAPIP());

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Página no encontrada.");
  });
  server.begin();
  Serial.println("[OK] Servidor web listo.");
}

// ── loop() ────────────────────────────────────────────────────────
void loop() {
  // 1. Ejecutar el comando que llegó por WebSocket (si hay)
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

  // 3. Limpiar conexiones WebSocket cerradas (una vez por segundo)
  static unsigned long ultimaLimpieza = 0;
  if (millis() - ultimaLimpieza > 1000) {
    ws.cleanupClients();
    ultimaLimpieza = millis();
  }

  delay(10);
}
