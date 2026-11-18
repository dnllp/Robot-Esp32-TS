# Robot ESP32 – Prácticas de Control Web

Prácticas de laboratorio para construir y controlar un **robot móvil diferencial con ESP32** desde el navegador del celular o la computadora, usando **WiFi (Access Point)**, un **servidor web asíncrono** y **WebSockets**.

Material de apoyo para la carrera de **Ingeniería en Sistemas Computacionales**.

| Práctica | Tema | Qué se aprende |
|---|---|---|
| [Práctica 1](#práctica-1--control-de-led-desde-la-web) | Control de LED | Access Point, servidor web, LittleFS, WebSocket |
| [Práctica 2](#práctica-2--movimiento-básico-de-motores) | Motores con L298N | PWM (LEDC), puente H, control remoto seguro |
| [Práctica 3](#práctica-3--control-remoto-completo-del-robot) | Robot completo | Integración, LEDs de estado, monitor de batería con ADC |

---

## Contenido

- [Estructura del repositorio](#estructura-del-repositorio)
- [Requisitos](#requisitos)
- [Preparación del entorno (una sola vez)](#preparación-del-entorno-una-sola-vez)
- [Cómo cargar cualquier práctica](#cómo-cargar-cualquier-práctica)
- [Práctica 1 – Control de LED desde la Web](#práctica-1--control-de-led-desde-la-web)
- [Práctica 2 – Movimiento Básico de Motores](#práctica-2--movimiento-básico-de-motores)
- [Práctica 3 – Control Remoto Completo del Robot](#práctica-3--control-remoto-completo-del-robot)
- [Solución de problemas](#solución-de-problemas)
- [Protocolo de mensajes WebSocket](#protocolo-de-mensajes-websocket)

---

## Estructura del repositorio

Arduino IDE exige que la carpeta del sketch se llame **igual que el archivo `.ino`**, y los archivos web deben ir en una subcarpeta **`data`**:

```
.
├── practica1/
│   ├── practica1.ino
│   └── data/
│       └── index.html
├── practica2/
│   ├── practica2.ino
│   └── data/
│       └── index.html
├── practica3/
│   ├── practica3.ino
│   └── data/
│       ├── index.html
│       ├── style.css
│       └── app.js
└── README.md
```

---

## Requisitos

### Software

| Componente | Versión | Dónde se instala |
|---|---|---|
| Arduino IDE | **2.x** | [arduino.cc/en/software](https://www.arduino.cc/en/software) |
| esp32 by Espressif Systems | **3.x** | Gestor de tarjetas |
| ESP Async WebServer (por **ESP32Async**) | la más reciente | Gestor de librerías |
| Async TCP (por **ESP32Async**) | la más reciente | Gestor de librerías |
| arduino-littlefs-upload | la más reciente | Plugin `.vsix` (ver abajo) |

> ⚠️ **No usar** las librerías originales de *me-no-dev* (descargadas como ZIP de GitHub). Están abandonadas y **no compilan** con el core ESP32 3.x.

### Hardware

| Material | Práctica |
|---|---|
| ESP32 DevKit V1 (ESP32-WROOM-32, 38 o 30 pines) | 1, 2, 3 |
| Cable USB **de datos** (no solo de carga) | 1, 2, 3 |
| Driver de motores L298N | 2, 3 |
| 2 motores DC con reductora + ruedas + chasis | 2, 3 |
| Batería 2S Li-ion (2 × 18650, 6.0–8.4 V) con portapilas | 2, 3 |
| Resistencias 47 kΩ y 10 kΩ, capacitor cerámico 100 nF | 3 |
| LED rojo, LED verde y 2 resistencias de 330 Ω | 3 |
| Cables Dupont y protoboard | 1, 2, 3 |

---

## Preparación del entorno (una sola vez)

### 1. Instalar el soporte para ESP32

1. Abrir Arduino IDE 2.x.
2. Ir a **Gestor de tarjetas** (icono lateral o *Herramientas → Placa → Gestor de tarjetas*).
3. Buscar **esp32** e instalar **esp32 by Espressif Systems**, versión **3.x**.

> Si no aparece, agregar esta URL en *Archivo → Preferencias → URLs adicionales del gestor de tarjetas*:
> `https://espressif.github.io/arduino-esp32/package_esp32_index.json`

### 2. Instalar las librerías

1. **Antes que nada**, cerrar el IDE y revisar la carpeta `Documentos\Arduino\libraries\`. Si existen carpetas como `ESPAsyncWebServer`, `AsyncTCP`, `ESPAsyncWebServer-master` o `AsyncTCP-master` instaladas desde un ZIP, **borrarlas**.
2. Abrir el IDE → **Gestor de librerías** (`Ctrl+Shift+I`).
3. Buscar **ESP Async WebServer** e instalar la del autor **ESP32Async**. Si pregunta por dependencias, elegir **Install all** (instala también **Async TCP**).
4. Si no instaló las dependencias, buscar **Async TCP** e instalar también la de **ESP32Async**.

> Hay varios *forks* con nombres casi iguales. Verificar siempre que el autor sea **ESP32Async**.

### 3. Instalar el plugin para subir archivos a LittleFS

El menú *"ESP32 LittleFS Data Upload"* solo existe en Arduino IDE 1.8.x. Para el IDE 2.x:

1. Descargar el archivo `.vsix` más reciente de [github.com/earlephilhower/arduino-littlefs-upload/releases](https://github.com/earlephilhower/arduino-littlefs-upload/releases).
2. Copiarlo en la carpeta de plugins (crearla si no existe):
   - **Windows:** `C:\Users\<usuario>\.arduinoIDE\plugins\`
   - **macOS / Linux:** `~/.arduinoIDE/plugins/`
3. Reiniciar el IDE.
4. Comprobar: `Ctrl+Shift+P` y escribir *LittleFS*. Debe aparecer **"Upload LittleFS to Pico/ESP8266/ESP32"**.

### 4. Configuración de la tarjeta

En el menú **Herramientas**:

| Opción | Valor |
|---|---|
| Placa | **ESP32 Dev Module** |
| Upload Speed | 921600 (bajar a 115200 si falla la carga) |
| Partition Scheme | **Default 4MB with spiffs** |
| PSRAM | Disabled |
| Puerto | El COM / `/dev/ttyUSB*` del ESP32 |

> Si el puerto no aparece, instalar el driver USB-serie de la placa: **CP210x** (Silicon Labs) o **CH340** (WCH), según el chip que tenga junto al conector USB.

---

## Cómo cargar cualquier práctica

Cada práctica tiene **dos cargas**: el sistema de archivos (página web) y el programa.

1. Abrir el `.ino` de la práctica.
2. **Configurar el equipo** al inicio del sketch (prácticas 1–3):
   ```cpp
   #define NUM_EQUIPO  "01"   // Número de equipo: 01, 02, 03...
   #define CANAL_WIFI  1      // Repartir entre equipos: 1, 6 u 11
   ```
   Así cada equipo tiene su propia red WiFi y se reduce la interferencia.
3. **Cerrar el Monitor Serial** (si está abierto, la carga falla).
4. Subir la carpeta `data`: `Ctrl+Shift+P` → **Upload LittleFS to Pico/ESP8266/ESP32**.
5. Subir el sketch: **Sketch → Subir** (`Ctrl+U`).
6. Abrir el **Monitor Serial a 115200 baud** y presionar el botón **EN** de la placa.
7. Conectar el celular o la computadora a la red WiFi que muestra el Monitor Serial.
8. Abrir el navegador en **http://192.168.4.1**.

> 📱 **En celulares:** desactivar los **datos móviles** antes de conectarse. Como la red del ESP32 no tiene internet, Android e iOS pueden abandonarla o enviar el tráfico por datos. Si el teléfono pregunta, elegir *"Mantener conexión"*.

> 💡 Solo hay que volver a subir LittleFS cuando cambian los archivos de `data`. Si solo cambia el `.ino`, basta con el paso 5.

---

## Práctica 1 – Control de LED desde la Web

### Objetivo

Crear una red WiFi propia con el ESP32, servir una página web desde su memoria (LittleFS) y encender o apagar un LED en tiempo real mediante WebSockets.

### Conexiones

No se necesita cableado: se usa el **LED integrado en GPIO 2** del ESP32 DevKit.

Opcional, LED externo:

```
GPIO 2 ──[330 Ω]──►|── GND
                  LED
```

> En placas **ESP32-C3** o **ESP32-S3** el LED integrado está en otro pin; cambiar `LED_PIN`.

### Datos de la red

| Parámetro | Valor |
|---|---|
| Red WiFi | `PracticaLED_E01` (según `NUM_EQUIPO`) |
| Contraseña | `practica1` |
| Dirección | http://192.168.4.1 |

### Procedimiento

1. Seguir los pasos de [Cómo cargar cualquier práctica](#cómo-cargar-cualquier-práctica).
2. Verificar en el Monitor Serial:
   ```
   = Práctica 1: Control de LED vía WebSocket =
   [OK] LittleFS montado correctamente.
   [WiFi] Access Point activo. SSID: PracticaLED_E01 (canal 1)
   [WiFi] Dirección IP del ESP32: 192.168.4.1
   [OK] Servidor web iniciado en http://192.168.4.1
   ```
3. Abrir la página. El indicador debe cambiar a **● Conectado**.
4. Presionar **Encender** y **Apagar**.

### Verificación

- [ ] El LED físico cambia al presionar los botones.
- [ ] El LED virtual de la página coincide con el físico.
- [ ] Al abrir la página en **dos dispositivos**, ambos se actualizan al mismo tiempo (`ws.textAll`).
- [ ] Al reiniciar el ESP32, la página muestra *Desconectado* y se reconecta sola.

### Preguntas para el reporte

1. ¿Qué ventaja tiene WebSocket frente a hacer una petición HTTP por cada clic?
2. ¿Por qué la página usa `location.host` en lugar de escribir la IP fija?
3. ¿Qué pasaría si dos alumnos presionan botones distintos al mismo tiempo?

---

## Práctica 2 – Movimiento Básico de Motores

### Objetivo

Controlar dos motores DC con el driver L298N usando PWM del ESP32: primero con una secuencia fija (Fase 1) y después de forma remota desde el navegador (Fase 2).

### Conexiones ESP32 → L298N

| ESP32 | L298N | Función |
|---|---|---|
| GPIO 26 | IN1 | Dirección motor A (rueda derecha) |
| GPIO 27 | IN2 | Dirección motor A |
| GPIO 14 | IN3 | Dirección motor B (rueda izquierda) |
| GPIO 32 | IN4 | Dirección motor B |
| GPIO 25 | ENA | PWM velocidad motor A |
| GPIO 33 | ENB | PWM velocidad motor B |
| GND | GND | **Tierra común (obligatoria)** |

Batería (+) → `+12V` del L298N · Batería (–) → `GND` del L298N.

> ⚠️ **Retirar los jumpers ENA y ENB** del módulo L298N. Si se dejan, los motores giran siempre a velocidad máxima y el PWM no tiene efecto.

> ⚠️ **No usar GPIO 12 para IN4.** Es un pin de arranque (*strapping*): si está en alto al encender, el ESP32 no arranca.

### Tabla de movimientos

| Comando | Movimiento | Motor A (der.) | Motor B (izq.) |
|---|---|---|---|
| `F` | Avanzar | Adelante 80 % | Adelante 80 % |
| `B` | Retroceder | Atrás 80 % | Atrás 80 % |
| `R` | Girar a la derecha | Detenido | Adelante 70 % |
| `L` | Girar a la izquierda | Adelante 70 % | Detenido |
| `P` | Girar en su eje | Adelante 50 % | Atrás 50 % |
| `S` | Detener | Detenido | Detenido |

### Fase 1 – Prueba directa (sin WiFi)

1. En el sketch, poner:
   ```cpp
   #define MODO_PRUEBA  1
   ```
2. **Levantar el robot** para que las ruedas giren en el aire.
3. Subir el sketch (no hace falta LittleFS en esta fase).
4. Observar en el Monitor Serial y en las ruedas la secuencia: avanzar → detener → retroceder → girar derecha → girar izquierda → girar en su eje.

**Si una rueda gira al revés:** intercambiar los dos cables de ese motor en las borneras del L298N. No hace falta cambiar el código.

### Fase 2 – Control remoto

1. Poner `#define MODO_PRUEBA  0`.
2. Seguir los pasos de [Cómo cargar cualquier práctica](#cómo-cargar-cualquier-práctica) (ahora sí, subir LittleFS).
3. Conectarse a `RobotP2_E01`, contraseña `practica2`, y abrir http://192.168.4.1.

**Controles:**

| Acción | Pantalla táctil / ratón | Teclado |
|---|---|---|
| Avanzar / Retroceder | Mantener ▲ / ▼ | ↑ / ↓ o W / S |
| Girar izq. / der. | Mantener ◀ / ▶ | ← / → o A / D |
| Girar en su eje | Mantener ⟲ | Q |
| Detener | Soltar o presionar ■ | Soltar o Espacio |

### Seguridad del control remoto

El robot **solo se mueve mientras el botón está presionado**:

- La página reenvía el comando cada **200 ms** mientras se mantiene presionado y envía `S` al soltar.
- El ESP32 se **detiene solo** si pasan **500 ms** sin recibir comandos (pérdida de WiFi, pestaña cerrada, celular fuera de alcance).
- También se detiene si la pestaña pierde el foco o se cierra la conexión.

### Verificación

- [ ] En la Fase 1 ambas ruedas giran en el sentido correcto en cada movimiento.
- [ ] En la Fase 2 el robot se detiene al soltar cualquier botón.
- [ ] Al **apagar el WiFi del celular** con el robot avanzando, se detiene en menos de 1 s.
- [ ] El Monitor Serial muestra `[SEGURIDAD] Sin comandos recientes: deteniendo.` en esa prueba.

### Preguntas para el reporte

1. Explica cómo funciona el puente H para invertir el giro del motor.
2. ¿Qué relación hay entre el *duty cycle* del PWM y la velocidad del motor?
3. ¿Por qué el manejador WebSocket no mueve los motores directamente, sino que deja el comando para `loop()`?
4. ¿Por qué es necesario el temporizador de seguridad si ya existe el evento de desconexión?

---

## Práctica 3 – Control Remoto Completo del Robot

### Objetivo

Integrar en un solo firmware el control de motores, LEDs indicadores de estado y el monitoreo del nivel de batería con el ADC, con una interfaz web separada en HTML, CSS y JavaScript.

### Conexiones

**Motores:** iguales que en la [Práctica 2](#conexiones-esp32--l298n).

**LEDs indicadores:**

```
GPIO 4 ──[330 Ω]──►|── GND     LED rojo:  sistema encendido
GPIO 2 ──[330 Ω]──►|── GND     LED verde: hay un cliente conectado
```

**Monitor de batería (divisor de tensión):**

```
Batería (+) ──[47 kΩ]──┬──────── GPIO 34
                       │    │
                    [10 kΩ] [100 nF]
                       │    │
GND ───────────────────┴────┘
```

- Factor del divisor: 10 / (47 + 10) = 0.175 → con 8.4 V la entrada recibe ≈ 1.47 V (seguro para el ESP32).
- El **capacitor de 100 nF** estabiliza la lectura; sin él los valores fluctúan.
- **GPIO 34** pertenece al **ADC1**, que funciona con el WiFi activo. Los pines del ADC2 no se pueden usar mientras el WiFi está encendido.

> ⚠️ Conectar **nunca** la batería directamente a un GPIO: el máximo es 3.3 V.

### Datos de la red

| Parámetro | Valor |
|---|---|
| Red WiFi | `RobotESP32_E01` (según `NUM_EQUIPO`) |
| Contraseña | `robot1234` |
| Dirección | http://192.168.4.1 |

### Procedimiento

1. Verificar que la carpeta `data` contiene **index.html, style.css y app.js**.
2. Seguir los pasos de [Cómo cargar cualquier práctica](#cómo-cargar-cualquier-práctica).
3. Verificar en el Monitor Serial que no aparezca ningún `[AVISO] Falta ...`.
4. Conectarse y abrir la página.

### Interfaz

- **Barra de batería:** se actualiza al conectarse y cada 10 s. Verde > 50 %, amarillo > 20 %, rojo ≤ 20 %.
- **Alerta:** aparece si la batería baja de **6.8 V**.
- **"Sin batería":** se muestra si la lectura es menor a 2 V (por ejemplo, robot alimentado solo por USB).
- **Movimiento:** debajo de los botones aparece el movimiento que el robot confirma.
- **Controles:** iguales que en la Práctica 2; en teclado, **P** es el giro en punto.

### Calibración de la batería

1. Medir la batería con un **multímetro**.
2. Comparar con el valor que muestra la página.
3. Si difieren, ajustar `BAT_DIVISOR` en el sketch:
   ```
   BAT_DIVISOR_nuevo = BAT_DIVISOR × (V_multímetro / V_página)
   ```

### Parámetros ajustables

| Constante | Valor | Descripción |
|---|---|---|
| `VEL_ALTA` | 204 (80 %) | Avance y retroceso |
| `VEL_GIRO` | 178 (70 %) | Giros con una rueda |
| `VEL_PUNTO` | 153 (60 %) | Giro en su eje |
| `BAT_BAJA` | 6.8 V | Umbral de alerta |
| `INTERVALO_BAT` | 10000 ms | Periodo de lectura de batería |
| `TIEMPO_SEGURIDAD_MS` | 500 ms | Paro automático sin comandos |

### Verificación

- [ ] El LED rojo enciende al energizar; el verde, al abrir la página.
- [ ] El LED verde se apaga al cerrar la página en todos los dispositivos.
- [ ] La tensión mostrada difiere menos de 0.2 V de la del multímetro.
- [ ] El robot se detiene al soltar cualquier botón, incluido el giro en punto.

### Preguntas para el reporte

1. Calcula el divisor necesario para medir una batería 3S (12.6 V máx.) sin superar 2.5 V en el pin.
2. ¿Por qué la lectura de batería baja cuando los motores están en marcha?
3. ¿Por qué se usa `analogReadMilliVolts()` en lugar de `analogRead() / 4095 × 3.3`?
4. ¿Qué ventajas tiene separar la interfaz en HTML, CSS y JavaScript?

---

## Solución de problemas

### Errores de compilación

| Mensaje | Causa | Solución |
|---|---|---|
| `'mbedtls_md5_starts_ret' was not declared` | Librería ESPAsyncWebServer vieja (me-no-dev) | Borrar la carpeta en `Documentos\Arduino\libraries\` e instalar la de **ESP32Async** |
| `Multiple libraries were found for "AsyncTCP.h"` | Hay dos versiones instaladas | Borrar la que **no** es de ESP32Async |
| `'ledcSetup' was not declared` | Código escrito para el core 2.x | Usar los sketches de este repositorio (API `ledcAttach`) |
| `#error "Esta práctica requiere el ESP32 Board Package v3.x"` | Core 2.x instalado | Actualizar *esp32 by Espressif* a 3.x |
| `#pragma message: BT: forcing BR/EDR...` | Aviso informativo del core | **No es un error**, ignorarlo |

### Errores al cargar

| Síntoma | Solución |
|---|---|
| `Failed to connect to ESP32: No serial data received` | Mantener presionado **BOOT** al iniciar la carga; probar otro cable USB (de datos) |
| No aparece el puerto | Instalar driver CP210x o CH340 |
| Falla la subida de LittleFS | Cerrar el Monitor Serial y volver a intentar |
| No aparece *Upload LittleFS* en `Ctrl+Shift+P` | Revisar que el `.vsix` esté en `.arduinoIDE/plugins/` y reiniciar el IDE |

### Errores en ejecución

| Monitor Serial | Causa | Solución |
|---|---|---|
| Solo aparece el arranque hasta `entry 0x...` y luego `Rebooting...` en bucle | El programa falla al iniciar (a veces es un programa viejo que quedó cargado porque el nuevo no compiló) | Revisar que el sketch compile sin errores y volver a subirlo |
| `Brownout detector was triggered` | Alimentación insuficiente al encender el WiFi | Otro cable USB, conectar directo a la PC sin hub |
| `[ERROR] No se pudo montar LittleFS` | No se subió la carpeta `data` | Subirla con *Upload LittleFS* |
| `[AVISO] Falta /index.html` | La carpeta `data` está mal ubicada o vacía | Debe estar **dentro** de la carpeta del sketch |
| La página no carga en el celular | El teléfono usa datos móviles | Desactivar datos móviles |
| Los motores no se mueven a velocidad baja | El L298N pierde ~2 V internamente | Aumentar `VEL_BAJA` / `VEL_PUNTO` |
| Los motores siempre van a máxima velocidad | Jumpers ENA/ENB puestos | Retirarlos |
| El robot se mueve raro o el ESP32 se reinicia al arrancar motores | Falta tierra común o la batería está baja | Unir GND del ESP32 y del L298N; cargar la batería |

---

## Protocolo de mensajes WebSocket

Todos los mensajes son texto plano por el endpoint `ws://192.168.4.1/ws`.

### Navegador → ESP32

| Mensaje | Práctica | Significado |
|---|---|---|
| `ON` / `OFF` | 1 | Encender / apagar LED |
| `F` `B` `L` `R` `P` | 2, 3 | Movimiento (se reenvía cada 200 ms mientras se mantiene) |
| `S` | 2, 3 | Detener |

### ESP32 → Navegador

| Mensaje | Práctica | Significado |
|---|---|---|
| `LED ENCENDIDO` / `LED APAGADO` | 1 | Estado actual del LED |
| `MOV:<c>` | 2, 3 | Movimiento confirmado (ej. `MOV:F`) |
| `ERR:<texto>` | 2 | Comando no reconocido |
| `BAT:<v>` | 3 | Tensión de batería (ej. `BAT:7.85`) o `BAT:NA` sin batería |
| `ALERTA:<texto>` | 3 | Mensaje de alerta para mostrar al usuario |

---

## Licencia

Material didáctico de uso académico. Agrega aquí la licencia del repositorio (por ejemplo, MIT o CC BY-SA 4.0).
