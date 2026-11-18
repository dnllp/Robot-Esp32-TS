// app.js  –  Práctica 3: Cliente WebSocket completo del Robot ESP32

// ── Elementos del DOM ──────────────────────────────────────────────
const elStatus  = document.getElementById('status');
const elBatBar  = document.getElementById('bat-bar');
const elBatTxt  = document.getElementById('bat-txt');
const elAlerta  = document.getElementById('alerta');
const elMov     = document.getElementById('movimiento');
const botones   = document.querySelectorAll('button[data-cmd]');

const VMAX = 8.4, VMIN = 6.0;   // Rango útil de batería 2S Li-ion

const NOMBRES = {
  F: 'Avanzando',
  B: 'Retrocediendo',
  L: 'Girando a la izquierda',
  R: 'Girando a la derecha',
  P: 'Giro en punto',
  S: 'Detenido'
};

// Mientras se mantiene un botón, el comando se reenvía cada
// REPETIR_MS. El ESP32 detiene el robot si deja de recibirlo
// (TIEMPO_SEGURIDAD_MS en el sketch).
const REPETIR_MS = 200;

let socket;
let retardo = 1000;
let cmdActivo = null;       // Comando que se está manteniendo
let temporizador = null;    // Reenvío periódico
let timerAlerta = null;

// ── Envío de comando al robot ──────────────────────────────────────
function enviarCmd(cmd) {
  if (socket && socket.readyState === WebSocket.OPEN) {
    socket.send(cmd);
  }
}

function marcarBoton(cmd) {
  botones.forEach(b => b.classList.toggle('activo', b.dataset.cmd === cmd));
}

// ── Empezar a mover (botón o tecla presionados) ───────────────────
function iniciar(cmd) {
  if (cmd === 'S') { soltar(); return; }
  if (cmdActivo === cmd) return;
  clearInterval(temporizador);
  cmdActivo = cmd;
  enviarCmd(cmd);
  temporizador = setInterval(() => enviarCmd(cmd), REPETIR_MS);
  marcarBoton(cmd);
}

// ── Detener (botón o tecla soltados) ──────────────────────────────
function soltar() {
  clearInterval(temporizador);
  temporizador = null;
  cmdActivo = null;
  enviarCmd('S');
  marcarBoton(null);
}

// ── Conexión WebSocket ─────────────────────────────────────────────
// location.host incluye el puerto si no es el 80
const WS_URL = `ws://${location.host}/ws`;

function habilitarBotones(activo) {
  botones.forEach(b => b.disabled = !activo);
}

function conectar() {
  socket = new WebSocket(WS_URL);

  socket.onopen = () => {
    elStatus.textContent = '● Conectado';
    elStatus.className   = 'status on';
    habilitarBotones(true);
    retardo = 1000;
    console.log('[WS] Conexión establecida con el robot.');
  };

  socket.onclose = () => {
    elStatus.textContent = '● Desconectado – reconectando...';
    elStatus.className   = 'status off';
    clearInterval(temporizador);
    temporizador = null;
    cmdActivo = null;
    marcarBoton(null);
    habilitarBotones(false);
    elMov.textContent = '–';
    setTimeout(conectar, retardo);
    retardo = Math.min(retardo * 2, 10000);
  };

  socket.onerror = (e) => console.error('[WS] Error:', e);

  socket.onmessage = (e) => {
    const msg = e.data;

    // Movimiento confirmado por el robot (formato: 'MOV:F')
    if (msg.startsWith('MOV:')) {
      const m = msg.charAt(4);
      elMov.textContent = NOMBRES[m] || m;
      return;
    }

    // Estado de batería (formato: 'BAT:7.85' o 'BAT:NA')
    if (msg.startsWith('BAT:')) {
      actualizarBateria(parseFloat(msg.substring(4)));
      return;
    }

    // Alerta del sistema
    if (msg.startsWith('ALERTA:')) {
      mostrarAlerta(msg.substring(7).trim());
      return;
    }

    console.log('[WS] Mensaje:', msg);
  };
}

// ── Actualizar indicador visual de batería ─────────────────────────
function actualizarBateria(v) {
  if (isNaN(v)) {                       // 'BAT:NA' → sin batería
    elBatTxt.textContent = 'Sin batería';
    elBatBar.style.width = '0%';
    return;
  }
  elBatTxt.textContent = v.toFixed(2) + ' V';
  const pct = Math.max(0, Math.min(100,
              ((v - VMIN) / (VMAX - VMIN)) * 100));
  elBatBar.style.width = pct + '%';
  // Color según nivel: verde > amarillo > rojo
  if (pct > 50)      elBatBar.style.background = '#00e676';
  else if (pct > 20) elBatBar.style.background = '#ffea00';
  else               elBatBar.style.background = '#ff1744';
}

// ── Mostrar alerta temporal ────────────────────────────────────────
function mostrarAlerta(msg) {
  elAlerta.textContent = '⚠ ' + msg;
  elAlerta.classList.remove('oculto');
  // Reiniciar el temporizador para que una alerta nueva no se oculte
  // antes de tiempo por el temporizador de la anterior
  clearTimeout(timerAlerta);
  timerAlerta = setTimeout(() => elAlerta.classList.add('oculto'), 6000);
}

// ── Eventos de puntero (mouse y pantalla táctil con un solo código) ─
botones.forEach(btn => {
  const cmd = btn.dataset.cmd;

  btn.addEventListener('pointerdown', (e) => {
    e.preventDefault();
    btn.setPointerCapture(e.pointerId);   // Recibir el 'soltar' aunque
    iniciar(cmd);                         // el dedo salga del botón
  });

  ['pointerup', 'pointercancel', 'lostpointercapture'].forEach(ev =>
    btn.addEventListener(ev, () => { if (cmdActivo === cmd) soltar(); })
  );

  btn.addEventListener('contextmenu', (e) => e.preventDefault());
});

// ── Control por teclado ────────────────────────────────────────────
const teclaCmd = {
  ArrowUp:    'F',  KeyW: 'F',
  ArrowDown:  'B',  KeyS: 'B',
  ArrowLeft:  'L',  KeyA: 'L',
  ArrowRight: 'R',  KeyD: 'R',
  KeyP:       'P',
  Space:      'S',
};

document.addEventListener('keydown', (e) => {
  const cmd = teclaCmd[e.code];
  if (!cmd) return;
  e.preventDefault();
  if (!e.repeat) iniciar(cmd);   // Ignorar la autorrepetición del teclado
});

document.addEventListener('keyup', (e) => {
  const cmd = teclaCmd[e.code];
  // Solo detener si se suelta la tecla del movimiento actual
  if (cmd && cmd === cmdActivo) soltar();
});

// ── Detener si la ventana pierde el foco o la pestaña se oculta ───
window.addEventListener('blur', () => { if (cmdActivo) soltar(); });
document.addEventListener('visibilitychange', () => {
  if (document.hidden && cmdActivo) soltar();
});

// ── Iniciar conexión al cargar la página ──────────────────────────
conectar();
