🫀 Monitor ECG — AD8232 · ESP32 · Node.js · MySQL

Sistema de monitoreo cardíaco en tiempo real con transmisión cifrada desde un ESP32 + AD8232, almacenamiento seguro en MySQL, visualización web profesional y exportación de datos.
Utiliza Node.js + WebSocket para la comunicación, cifrado en tránsito (AES-128-CBC), cifrado en reposo (AES-256-GCM) e integridad HMAC-SHA256 por cada registro.

──────────────────────────────────────────────────────────────────────────────

📁 Estructura del proyecto

ecg-monitor/
├── public/
│   └── index.html          ← Interfaz web (gráfica ECG, controles, exportación)
├── server.js               ← Servidor Node.js + WebSocket + API REST
├── esquema_bd.sql          ← (opcional) Script de creación de tablas
└── README.md

──────────────────────────────────────────────────────────────────────────────

⚙️ Cómo funciona

ESP32+AD8232 ──(AES-128-CBC)──> WebSocket ──> server.js
                                      │
                                Descifra tránsito
                                      │
                          ┌───────────┴───────────┐
                          │                       │
                    Cifra en reposo         Broadcast a navegadores
                    (AES-256-GCM)           (datos en claro)
                          │                       │
                      MySQL                   Interfaz web
                   (muestras + HMAC)         (Chart.js, FC, export)

1. ESP32 lee el sensor AD8232 a 200 Hz y cifra cada bloque de muestras con AES-128-CBC.
2. Envía por WebSocket el paquete binario "IV (16 bytes) + datos cifrados".
3. El servidor descifra, aplica padding PKCS#7 y obtiene las muestras (valores ADC).
4. Cifra cada muestra por separado con AES-256-GCM, calcula un HMAC-SHA256 del registro completo y lo guarda en MySQL.
5. Retransmite las muestras en claro a todos los navegadores conectados para la visualización en tiempo real.
6. La interfaz web dibuja la señal ECG, calcula frecuencia cardíaca, permite ajustar la vista, pausar, y exportar datos (desde la BD o desde el buffer local).

──────────────────────────────────────────────────────────────────────────────

🧱 Dependencias

Node.js (servidor)

  npm install express ws mysql2 exceljs

- ws: módulo WebSocket
- mysql2: para MySQL
- exceljs: exportar Excel (solo en el servidor; el cliente usa SheetJS por CDN)

Frontend
- Chart.js (CDN)
- SheetJS (CDN, para exportación local de respaldo)
- Fuentes de Google Fonts (Share Tech Mono, Rajdhani)

No se requieren dependencias Python.

──────────────────────────────────────────────────────────────────────────────

🗄️ Configuración de la base de datos

Crear una base de datos MySQL (por ejemplo ecg_db) y ejecutar:

CREATE TABLE sesiones (
  id INT AUTO_INCREMENT PRIMARY KEY,
  paciente VARCHAR(100) DEFAULT 'Anónimo',
  notas TEXT,
  inicio TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  fin TIMESTAMP NULL,
  activa BOOLEAN DEFAULT TRUE
);

CREATE TABLE lecturas (
  id INT AUTO_INCREMENT PRIMARY KEY,
  sesion_id INT NOT NULL,
  timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  muestra_cifrada TEXT NOT NULL,
  hmac VARCHAR(128) NOT NULL DEFAULT 'pendiente',
  FOREIGN KEY (sesion_id) REFERENCES sesiones(id)
);

CREATE TABLE audit_log (
  id INT AUTO_INCREMENT PRIMARY KEY,
  evento VARCHAR(50),
  sesion_id INT,
  detalle TEXT,
  ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

El servidor se conecta usando variables de entorno (o valores por defecto en server.js):
DB_HOST, DB_USER, DB_PASSWORD, DB_NAME

──────────────────────────────────────────────────────────────────────────────

🔐 Capas de seguridad

┌─────────────────┬──────────────────┬──────────────────────────────────────────────┬───────────────────────────────┐
│ Capa            │ Algoritmo        │ Clave / Derivación                           │ Propósito                     │
├─────────────────┼──────────────────┼──────────────────────────────────────────────┼───────────────────────────────┤
│ Tránsito        │ AES-128-CBC      │ Clave fija de 16 bytes (AES_TRANSIT_KEY)     │ Evitar escuchas en red WiFi   │
│ Reposo          │ AES-256-GCM      │ Derivada con scryptSync de DB_PASSPHRASE      │ Cada muestra cifrada en BD    │
│ Integridad      │ HMAC-SHA256      │ Derivada con scryptSync de HMAC_PASSPHRASE    │ Detectar alteraciones en fila │
└─────────────────┴──────────────────┴──────────────────────────────────────────────┴───────────────────────────────┘

- Clave de tránsito: definida en server.js como AES_TRANSIT_KEY (16 bytes fijos).
- Clave de reposo: derivada mediante crypto.scryptSync a partir de la variable de entorno DB_PASSPHRASE.
- Clave HMAC: derivada de la variable de entorno HMAC_PASSPHRASE.

El HMAC se calcula sobre la concatenación:
  id | timestamp | sesion_id | muestra_cifrada

La verificación se realiza con el endpoint /api/verificar-integridad.

──────────────────────────────────────────────────────────────────────────────

🌐 Endpoints REST

┌──────────┬──────────────────────────────┬───────────────────────────────────────────────────┐
│ Método   │ Ruta                         │ Descripción                                       │
├──────────┼──────────────────────────────┼───────────────────────────────────────────────────┤
│ POST     │ /api/sesion/abrir            │ Abre una nueva sesión. Body: { paciente, notas }  │
│ POST     │ /api/sesion/cerrar           │ Cierra la sesión actual.                          │
│ GET      │ /api/sesiones                │ Lista las últimas 50 sesiones.                    │
│ GET      │ /api/verificar-integridad    │ Verifica HMAC de los últimos registros. ?limit=n  │
│ GET      │ /api/export/excel            │ Descarga Excel con datos históricos. ?limit=n&from=&to=  │
│ GET      │ /api/audit                   │ Consulta el log de auditoría. ?limit=n            │
└──────────┴──────────────────────────────┴───────────────────────────────────────────────────┘

──────────────────────────────────────────────────────────────────────────────

📡 Protocolo WebSocket

• Identificación del ESP32: el ESP32 envía el texto "ESP32". El servidor lo marca
  y abre una sesión automáticamente.
• Datos binarios: el ESP32 envía paquetes con la estructura:
    [IV 16 bytes] [datos cifrados con AES-128-CBC]
  Los datos descifrados son un array de enteros de 16 bits (little-endian) que
  representan lecturas del ADC (0-4095).
• Broadcast a navegadores: el servidor envía a todos los clientes no-ESP32:
    { "type": "ecg", "data": [2048, 2056, 2034, ...] }
  También envía { "type": "status", "connected": true/false } al cambiar el estado.

──────────────────────────────────────────────────────────────────────────────

📊 Interfaz web (index.html)

- Gráfica de línea en tiempo real con Chart.js, fondo oscuro estilo monitor médico.
- Ajustes: ventana de tiempo (1 s a 10 s), escala vertical (auto/fija/completa),
  grosor de trazo.
- Indicadores: conexión del ESP32, frecuencia cardíaca (detección de picos),
  amplitud pico-pico (voltios), último valor ADC.
- Pausa / reanudación de la gráfica.
- Exportación de datos:
  • Intenta descargar desde la base de datos con la API REST (/api/export/excel).
  • Si falla (error del servidor, sin datos), exporta automáticamente los
    puntos del buffer local usando SheetJS.
- Barra de estado con hora, método de exportación y capas de seguridad.

──────────────────────────────────────────────────────────────────────────────

🔌 Conexión del AD8232 al ESP32

┌─────────────┬────────────────┐
│ Pin AD8232  │ Pin ESP32      │
├─────────────┼────────────────┤
│ GND         │ GND            │
│ 3.3V        │ 3.3V           │
│ OUTPUT      │ GPIO34 (ADC)   │
│ LO+         │ GPIO32 (opc.)  │
│ LO-         │ GPIO33 (opc.)  │
└─────────────┴────────────────┘

Electrodos según derivación I de Einthoven:
  Brazo derecho (RA), brazo izquierdo (LA), pierna derecha (RL).
La señal se muestrea a 200 Hz en el firmware del ESP32.

──────────────────────────────────────────────────────────────────────────────

🔧 Firmware del ESP32 (consideraciones)

- Conectar a la misma red WiFi que el servidor.
- IP del servidor configurable (ej. en secrets.h).
- Clave AES-128 fija idéntica a AES_TRANSIT_KEY del servidor.
- Al iniciar, enviar el texto "ESP32" por WebSocket para identificarse.
- Leer el ADC con un timer a 200 Hz, acumular N muestras, cifrarlas en modo CBC
  con un IV aleatorio y enviar el paquete binario por WebSocket.

──────────────────────────────────────────────────────────────────────────────

🚀 Ejecutar el servidor

1. Clona o copia los archivos del proyecto.
2. Instala las dependencias:
     npm install
3. Asegúrate de tener MySQL corriendo y las tablas creadas.
4. (Opcional) Define variables de entorno:
     DB_HOST, DB_USER, DB_PASSWORD, DB_NAME, DB_PASSPHRASE, HMAC_PASSPHRASE.
5. Inicia el servidor:
     node server.js
6. Abre http://localhost:3000 en el navegador.

──────────────────────────────────────────────────────────────────────────────

📝 Notas finales

- El servidor genera automáticamente una nueva sesión en MySQL al conectar un
  ESP32 si no hay una activa.
- Todos los timestamps se almacenan en UTC.
- Se mantiene un log de auditoría (audit_log) con eventos importantes.
- Las claves de cifrado en server.js son ejemplos didácticos. En producción
  deben manejarse con variables de entorno seguras.
- La exportación híbrida (BD + buffer local) garantiza que siempre puedas
  descargar datos, incluso si la base de datos falla temporalmente.
- La interfaz está en español, diseñada para simular un monitor médico.

──────────────────────────────────────────────────────────────────────────────

📄 Licencia

Uso educativo y experimental. No apto para diagnóstico médico sin las
certificaciones correspondientes.
